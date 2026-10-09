const test=require('node:test'), assert=require('node:assert/strict');
const vm=require('node:vm'), esbuild=require('esbuild');
const {CATALOG}=require('../src/item-catalog.cjs');
const uiProfile=require('../src/ui-interruptions.cjs');
// Compile the ACTUAL agent with fake bridges; these tests simulate contracts,
// not a live Android process and not a real server-side catch.
const compiled=esbuild.build({entryPoints:['src/agent.ts'],bundle:true,write:false,
  format:'iife',platform:'neutral',plugins:[{name:'fake-bridges',setup(build){
    build.onResolve({filter:/^frida-(il2cpp|java)-bridge$/},args=>({path:args.path,namespace:'mock'}));
    build.onLoad({filter:/.*/,namespace:'mock'},args=>({contents:args.path==='frida-java-bridge'?
      'export default globalThis.__mockJava;':'globalThis.Il2Cpp=globalThis.__mockIl2Cpp;',loader:'js'}));
  }}]}).then(result=>result.outputFiles[0].text);
// Additional integration-only fake; the independent native adapter itself is
// tested against a full-width JNI table in native-location.test.cjs.
const compiledLocation=esbuild.build({entryPoints:['src/agent.ts'],bundle:true,write:false,
  format:'iife',platform:'neutral',plugins:[{name:'fake-location-lifecycle',setup(build){
    build.onResolve({filter:/^frida-(il2cpp|java)-bridge$/},args=>({path:args.path,namespace:'mock'}));
    build.onLoad({filter:/.*/,namespace:'mock'},args=>({contents:args.path==='frida-java-bridge'?
      'export default globalThis.__mockJava;':'globalThis.Il2Cpp=globalThis.__mockIl2Cpp;',loader:'js'}));
    build.onLoad({filter:/native-location\.cjs$/},()=>({contents:'module.exports=globalThis.__mockNativeLocation;',loader:'js'}));
  }}]}).then(result=>result.outputFiles[0].text);

async function harness(overrides={}) {
  let uiCallbacks=null,uiDetached=false,uiOriginalOpens=0;
  const uiActions=[];
  let now=10000, focused=true, detached=false, callback, controlSequence=0;
  const events=[],calls=[],deferred=[],intervals=[],rpcRequests=[],nativeBindings=[],gcCalls=[];
  const objects=new Map(),nativeHandles=new Map(),exports=new Map();
  let objectAddress=0x7e4000000000n,handleAddress=0x7e4474a02800n;
  class Pointer {
    constructor(value){this.value=BigInt(value);}
    isNull(){return this.value===0n;}
    equals(other){return this.value===other.value;}
    toString(){return '0x'+this.value.toString(16);}
  }
  class Obj {
    constructor(name,fields={},methods={}) {
      if(name instanceof Pointer){const existing=objects.get(name.toString());assert.ok(existing,'Unknown native object '+name);return existing;}
      this.class={name,fullName:name,isValueType:false};this.fields=fields;this.methods=methods;
      this.handle=new Pointer(objectAddress);objectAddress+=0x100n;objects.set(this.handle.toString(),this);
    }
    isNull(){return false;}
    ref(){throw Error('Legacy uint32 GCHandle must never be used');}
    field(name){if(!(name in this.fields))throw Error('Missing field '+name);const self=this;
      return {offset:24,get value(){return self.fields[name];},set value(value){self.fields[name]=value;}};}
    tryField(name){return name in this.fields?this.field(name):null;}
    method(name){if(!(name in this.methods))throw Error('Missing method '+name);const fn=this.methods[name];
      const result={virtualAddress:{isNull:()=>false},returnType:{name:
        ['get_Count','get_MaxPokemon'].includes(name)?'System.Int32':'System.Object'},invoke:(...args)=>fn(...args),
        overload:()=>result,inflate:()=>result,parameters:name==='UseItemForEncounter'?
          [{type:{name:'Holoholo.Rpc.Item'}},{type:{name:overrides.badBerrySignature?'BadType':'Niantic.Holoholo.Map.IMapPokemon'}}]:[{type:{name:
          ['GetPokemon','GetIsBuddy'].includes(name)?'System.UInt64':name==='ReleasePokemon'?'Holoholo.Rpc.PokemonProto':
          name==='GetClass'?'IGameMasterData':'Niantic.Holoholo.Encounter.PokeballThrow'}}]};return result;}
    tryMethod(name){return name in this.methods?this.method(name):null;}
  }
  Obj.headerSize=16;
  function NativeFunction(address,returnType,argTypes) {
    const name=exports.get(address.toString());assert.ok(name);nativeBindings.push({name,returnType,argTypes});
    return (...args)=>{
      gcCalls.push({name,args:args.map(x=>x instanceof Pointer?x.toString():x)});
      if(name==='il2cpp_gchandle_new'||name==='il2cpp_gchandle_new_weakref') {
        assert.equal(returnType,'pointer');assert.deepEqual(Array.from(argTypes),['pointer','bool']);
        assert.ok(args[0] instanceof Pointer);assert.equal(args[1],name.endsWith('weakref')?0:1);
        const token=new Pointer(handleAddress);handleAddress+=8n;nativeHandles.set(token.toString(),args[0]);
        return overrides.gcNewReturnsNumber?Number(token.value&0xffffffffn):token;
      }
      assert.ok(args[0] instanceof Pointer);assert.deepEqual(Array.from(argTypes),['pointer']);
      const key=args[0].toString();assert.ok(nativeHandles.has(key),'Invalid/truncated handle '+key);
      if(name==='il2cpp_gchandle_get_target') {
        assert.equal(returnType,'pointer');return overrides.gcWrongTarget?new Pointer(0):nativeHandles.get(key);
      }
      assert.equal(returnType,'void');
      if(overrides.gcFreeFailure)throw Error('Simulated native free failure');
      nativeHandles.delete(key);
    };
  }
  // The real bridge's ValueType and Object are separate NativeStruct
  // subclasses. A struct must NOT accidentally satisfy instanceof Object.
  class ValueType {
    constructor(name,fields={},methods={}) {
      this.type={name};this.storage=new Obj(name,fields,methods);
    }
    field(name){return this.storage.field(name);}
    box(){const boxed=new Obj(this.type.name,this.storage.fields,this.storage.methods);
      boxed.class.isValueType=true;boxed.unbox=()=>this;return boxed;}
  }
  let collectionReads=0,collectionMoves=0;
  function sequence(items,collection=false) {return new Obj('List',{}, {get_Count:()=>overrides.collectionCount??items.length,GetEnumerator(){let i=-1;
    return new Obj('Enumerator',{}, {MoveNext:()=>{if(collection)collectionMoves++;return ++i<items.length;},get_Current:()=>items[i],Dispose(){}});}});}
  const responses=[];
  function promise(kind) {const p=new Obj('Promise',{completeCalled:kind==='capture'&&!!overrides.precompletedCapture,errorCalled:false,completedValue:null});
    if(p.fields.completeCalled)p.fields.completedValue=new Obj('Holoholo.Rpc.CatchPokemonOutProto',
      {Status:1,CapturedPokemonId:{toString:()=>ownedId}});
    responses.push({kind,p});return p;}
  function location(fields={}) {
    const value=new ValueType(overrides.locationType??'Niantic.LatLng',{Latitude:0,Longitude:0,...fields});
    if(overrides.locationMode==='boxed')return value.box();
    if(overrides.locationMode==='null'){const absent=new Obj('Niantic.LatLng');absent.isNull=()=>true;return absent;}
    if(overrides.locationMode==='plain')return {Latitude:0,Longitude:0};
    return value;
  }
  const playerLocation=location(overrides.playerLocation);
  const throwData=[];
  function recordThrow(data) {
    assert.ok(data instanceof ValueType);assert.ok(!(data instanceof Obj));
    throwData.push(Object.fromEntries(['Missed','HitBullseye','Spinning','ReticleSize'].map(name=>[name,data.field(name).value])));
  }
  let encounterId='111111111111111111';
  const pokemon=new Obj('WildMapPokemon',{}, {get_Destroyed:()=>overrides.pokemonDestroyed??false,get_Location:()=>location(overrides.pokemonLocation),
    get_EncounterId:()=>({toString:()=>encounterId}),SendEncounterRequest(){calls.push('encounter');return promise('encounter');},
    TryCapture(data){recordThrow(data);
      calls.push('capture');return promise('capture');}});
  const mapPokemon=overrides.mapPokemon?overrides.mapPokemon.map(spec=>new Obj(spec.className??'WildMapPokemon',{}, {
    get_Destroyed:()=>spec.destroyed??false,get_Location:()=>location(spec.location),
    get_EncounterId:()=>({toString:()=>spec.id}),
    SendEncounterRequest(){calls.push('encounter');return promise('encounter');},
    TryCapture(data){recordThrow(data);calls.push('capture');return promise('capture');}
  })):[pokemon];
  const stopObject=new Obj('Pokestop',{}, {get_Id:()=>({content:'stop-1'}),get_Location:()=>location(overrides.stopLocation)});
  const inventory={PokeBall:10,GreatBall:2,UltraBall:3,Potion:100,SuperPotion:0,
    HyperPotion:0,MaxPotion:0,Revive:0,MaxRevive:0,RazzBerry:0,NanabBerry:0,PinapBerry:0};
  if(overrides.inventory)Object.assign(inventory,overrides.inventory);
  let ownedId=overrides.ownedId??'18446744073709551614';
  const owned=new Obj('Holoholo.Rpc.PokemonProto',{id_:{toString:()=>ownedId},pokemonId_:25,
    individualAttack_:15,individualDefense_:15,individualStamina_:15,favorite_:false,isEgg_:false,
    deployedFortId_:{content:''},deployedStationId_:{content:''},isFusion_:false,limitedPokemonIdentifier_:{content:''},...overrides.ownedFields},
    {get_IsShinyPokemon:()=>overrides.shiny??false,GetClass:()=>overrides.pokemonClass??0});
  const existing=new Obj('Holoholo.Rpc.PokemonProto',{...owned.fields,id_:{toString:()=> '101'}},owned.methods);
  const collectionItems=overrides.transferEnabled&&!overrides.preexistingOwned ? [existing] : [owned];
  if(overrides.collectionLength) {
    collectionItems.length=0;
    for(let i=1;i<=overrides.collectionLength;i++)collectionItems.push(new Obj('Holoholo.Rpc.PokemonProto',
      {...owned.fields,id_:{toString:()=>String(i)}},owned.methods));
  }
  let removed=false;
  const ownedLookups=[];
  const pokemonBag=new Obj('PokemonBagImpl',{}, {get_Count:()=>overrides.pokemonCount??50,get_AllPokemon(){collectionReads++;
    if(overrides.collectionAccessThrows)throw Error('Active collection scan forbidden');return sequence(collectionItems,true);},GetPokemon(id){
    ownedLookups.push(id.toString());assert.equal(typeof id,'object');
    overrides.onOwnedLookup?.(id.toString(),ownedLookups.length,owned.fields);
    const original=collectionItems.find(item=>item.fields.id_.toString()===id.toString());
    if(original&&(!overrides.ownedMissing||id.toString()!==ownedId))return original;
    if(overrides.ownedMissing||removed||id.toString()!==ownedId){const absent=new Obj('Absent');absent.isNull=()=>true;return absent;}
    return owned;
  },ReleasePokemon(value){assert.equal(value,owned,'Only captured exact Pokemon may be released');
    calls.push('transfer');return promise('transfer');}});
  let itemCountReads=0;
  const berryRequests=[];
  const bagMethods={GetItemCount:(item)=>{itemCountReads++;
    return inventory[CATALOG.find(entry=>entry.id===item)?.name]??0;},GetTotalItems:()=>overrides.totalItems??115};
  if(!overrides.missingBerryMethod)bagMethods.UseItemForEncounter=(item,target)=>{
    if(overrides.berryCallThrows)throw Error('Simulated native berry failure');
    berryRequests.push({item,id:target.method('get_EncounterId').invoke().toString()});calls.push('berry');
    return promise('berry');
  };
  const bag=new Obj('ItemBagImpl',{},bagMethods);
  const directory=new Obj('Directory',{}, {get_AllPokemon:()=>sequence(mapPokemon),get_ActivePokestops:()=>sequence([stopObject])});
  const handler=new Obj('RpcHandler',{}, {SendRpc(id,request){rpcRequests.push({id,fields:{...request.fields}});
    calls.push(id===101?'spin':'discard');return promise(id===101?'spin':'discard');}});
  const services={'Niantic.Holoholo.IItemBag':bag,'Niantic.Holoholo.IPokemonBag':pokemonBag,'Niantic.Holoholo.Map.IMapPlaceDirectoryService':directory,
    'Niantic.Holoholo.Services.IItemBagService':new Obj('BagService',{}, {get_IsFull:()=>overrides.bagFull===true}),
    'Niantic.Holoholo.Rpc.IRpcHandler':handler,'Niantic.Holoholo.Avatar.IMapAvatar':new Obj('Avatar',{}, {get_Location:()=>playerLocation}),
    'IGameMasterData':new Obj('GameMasterData'),
    'Niantic.Holoholo.Buddy.IBuddyPokemonService':new Obj('BuddyPokemonService',{}, {GetIsBuddy:()=>overrides.buddy??false}),
    'Niantic.Holoholo.IPlayerService':new Obj('PlayerService',{}, {get_PlayerId:()=>({content:overrides.playerId??'mock_player_A'}),
      get_MaxItems:()=>overrides.maxItems??1000,get_MaxPokemon:()=>overrides.maxPokemon??1000,
      get_BagIsFull:()=>overrides.itemBagFull??false})};
  if(overrides.missingService){const absent=new Obj('Absent');absent.isNull=()=>true;services[overrides.missingService]=absent;}
  const container=new Obj('Container',{}, {TryResolve:type=>services[type.name]});
  const context=new Obj('SceneContext',{_container:container});
  function klass(name) {
    if(name===uiProfile.GUI_CLASS||uiProfile.TARGETS.some(target=>target.name===name)) {
      const type={fullName:name,hierarchy:()=>[type,{fullName:uiProfile.GUI_CLASS}],tryField(field){
        return field==='currentState'?{name:field,offset:112,isStatic:false,
          type:{class:{isEnum:true,baseType:{name:'System.Int32'}}}}:
          field==='m_CachedPtr'?{type:{name:'System.IntPtr'},isStatic:false}:null;
      },tryMethod(method){
        if(overrides.missingGuiProfile)return null;
        return {isStatic:false,parameterCount:0,virtualAddress:new Pointer(0x999n),
          returnType:{name:method==='get_IsTopGui'?'System.Boolean':'System.Void'},
          bind:controller=>controller.method(method)};
      }};return type;
    }
    const protoFields={Id:null,PlayerLatDegrees:0,PlayerLngDegrees:0,FortLatDegrees:0,FortLngDegrees:0,Item:0,Count:0};
    return {fullName:name,type:{object:{name}},field:field=>{
      if(name==='Holoholo.Rpc.Item'&&overrides.missingItemFields?.includes(field))throw Error('Missing enum '+field);
      return {value:name==='Holoholo.Rpc.Item'?(overrides.itemIdOverrides?.[field]??CATALOG.find(entry=>entry.name===field)?.id):
        {name:field},offset:overrides.badLayout?0:24};},inflate(){return this;},
      alloc:()=>new Obj(name,{...protoFields},{'.ctor':()=>{}}),
      method(method) {const obj=new Obj(name,{}, {[method]:()=>method==='First'?context:
        method==='GetAllSceneContexts'?sequence(overrides.sceneRemoved?[]:[context]):new ValueType('Throw',{Missed:true,HitBullseye:false,Spinning:false,ReticleSize:1})});
        return obj.method(method);}
    };
  }
  const fake={Object:Obj,ValueType,domain:{assembly:()=>({image:{class:klass}})},
    application:{identifier:'com.nianticlabs.pokemongo',version:'0.431.0'},unityVersion:overrides.unityVersion??'6000.3.21f1',
    module:{getExportByName(name){const address=new Pointer(BigInt(exports.size+1));exports.set(address.toString(),name);return address;}},
    perform:async fn=>fn(),string:content=>({content})};
  const sandbox={__mockIl2Cpp:fake,__mockJava:{perform:fn=>fn(),performNow:fn=>fn(),
    use:()=>({currentActivity:{value:{hasWindowFocus:()=>focused}}})},
    NativePointer:Pointer,NativeFunction,Module:{},Memory:{},ptr:value=>new Pointer(value),
    __mockNativeLocation:{installNativeLocation(_runtime,report){
      if(overrides.locationInstallFailure)throw Error('Simulated JNI resolution failure');
      report('native_location_ready',{implementation:'rootcatch-jni-v1'});
      return {close(){return {passed:!overrides.locationCleanupFailure,remaining:overrides.locationCleanupFailure?1:0,
        errors:overrides.locationCleanupFailure?['Simulated JNI detach failure']:[]};},summary(){}};
    }},
    UInt64:class {constructor(value){this.value=String(value);}toString(){return this.value;}},
    Process:{id:123,arch:'arm64',pointerSize:8,getCurrentThreadId:()=>321},rpc:{exports:{}},send:msg=>{
      assert.equal(msg[0],'frida:stdout');events.push(JSON.parse(msg[1]).payload);
    },
    Interceptor:{attach:(_address,handlers)=>{
      if(handlers.onEnter){uiCallbacks=handlers;return {detach(){uiDetached=true;}};}
      callback=handlers.onLeave;return {detach(){detached=true;}};
    }},
    Date:{now:()=>now},setImmediate:fn=>deferred.push(fn),setInterval:fn=>intervals.push(fn),console};
  vm.runInNewContext(await (overrides.fakeLocationLifecycle?compiledLocation:compiled),sandbox);
  const params={gamePackage:'com.nianticlabs.pokemongo',version:'0.431.0',maxThrows:2,sessionId:'mock_session_A',
    actionDelayMs:250,autoCatch:true,autoSpin:false,cleanBag:false,discard:{Potion:50},maxDiscardedItems:0,
    cleanupTrigger:'EXCESS_FROM_START',...overrides};
  assert.equal(sandbox.rpc.exports.init('early',params),undefined,'init must not block CLI stdin');
  deferred.forEach(fn=>fn());await new Promise(resolve=>setImmediate(resolve));
  function command(value){sandbox.rpc.exports.onFridaStdin(JSON.stringify(value));}
  function tick(ms=300,focus=true,hostFocus=focus,shade=false,mapVisible=true){now+=ms;focused=focus;
    command({type:'heartbeat',foreground:hostFocus,notificationShade:shade,
      sequence:++controlSequence,sentAtElapsedMs:now});if(callback&&!detached)callback({toInt32:()=>mapVisible?1:0});}
  function complete(fields={}){const next=responses.shift();assert.ok(next,'Expected pending game promise');
    next.p.fields.completedValue=new Obj(next.kind==='transfer'?'Holoholo.Rpc.ReleasePokemonOutProto':
      next.kind==='capture'?(overrides.captureResponseClass??'Holoholo.Rpc.CatchPokemonOutProto'):
      next.kind==='discard'?(overrides.discardResponseClass??'Holoholo.Rpc.RecycleItemOutProto'):
      next.kind==='berry'?(overrides.berryResponseClass??'Holoholo.Rpc.UseItemEncounterOutProto'):
      next.kind==='encounter'?(overrides.encounterResponseClass??'Niantic.Holoholo.Map.PokemonEncounterResponse'):'Result',
      next.kind==='encounter'?{InitialActiveItem:0,...fields}:fields);
    next.p.fields.completeCalled=true;return next.kind;}
  function openGui(name,extra={}) {
    const gui=new Obj(name,{currentState:1,m_CachedPtr:new Pointer(1)},{get_IsTopGui:()=>extra.top!==false});
    const target=uiProfile.TARGETS.find(value=>value.name===name);
    if(target)gui.methods[target.action]=()=>{
      uiActions.push({name,action:target.action,originalOpens:uiOriginalOpens});
      overrides.onUiAction?.();
      gui.fields.currentState=target.action==='Click'?2:3;
    };
    const invocation={};if(uiCallbacks&&!uiDetached)uiCallbacks.onEnter.call(invocation,[gui.handle]);
    uiOriginalOpens++;gui.fields.currentState=2;
    if(uiCallbacks&&!uiDetached)uiCallbacks.onLeave.call(invocation);return gui;
  }
  return {events,calls,tick,complete,inventory,command,responses,rpcRequests,nativeBindings,gcCalls,ownedLookups,throwData,berryRequests,
    openGui,uiActions,get uiDetached(){return uiDetached;},
    shadeTick(ms=300){tick(ms,false,true,true);},
    tickWithoutHeartbeat(ms=300){now+=ms;if(callback&&!detached)callback({toInt32:()=>1});},
    get collectionReads(){return collectionReads;},get collectionMoves(){return collectionMoves;},
    get itemCountReads(){return itemCountReads;},
    get ownedId(){return ownedId;},setOwnedId(value){ownedId=value;removed=false;},
    setEncounterId(value){encounterId=value;},
    setOwnedMissing(value){overrides.ownedMissing=value;},
    ownedFields:owned.fields,setShiny(value){overrides.shiny=value;},setBuddy(value){overrides.buddy=value;},
    removeOwned(){removed=true;},
    setBagFull(value){overrides.bagFull=value;},
    setPokemonCount(value){overrides.pokemonCount=value;},setPlayerId(value){overrides.playerId=value;},
    setPokemonDestroyed(value){overrides.pokemonDestroyed=value;},
    setPlayerLocation(fields){for(const [name,value] of Object.entries(fields))playerLocation.field(name).value=value;},
    removeScene(){overrides.sceneRemoved=true;},
    replacePlayerService(){services['Niantic.Holoholo.IPlayerService']=new Obj('ReplacementPlayerService',{},
      {get_PlayerId:()=>({content:overrides.playerId??'mock_player_A'})});},
    replaceMapService(){services['Niantic.Holoholo.Map.IMapPlaceDirectoryService']=new Obj('ReplacementDirectory');},
    setFreeFailure(){overrides.gcFreeFailure=true;},
    get remainingHandles(){return nativeHandles.size;},
    locationIsUnboxedStruct:playerLocation instanceof ValueType && !(playerLocation instanceof Obj),
    watchdog(){now+=3100;intervals.forEach(fn=>fn());},get detached(){return detached;}};
}
test('read-only recognizes services but sends no game action',async()=>{
  const h=await harness({readOnly:true,autoSpin:true,cleanBag:true});h.tick();h.tick();
  assert.ok(h.locationIsUnboxedStruct,'Regress: native LatLng is not an Object');
  assert.ok(h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);
  assert.ok(h.events.some(e=>e.event==='location_validated'));
  assert.ok(!h.events.some(e=>e.event==='error'));
  const check=h.events.find(e=>e.event==='gc_validation');assert.ok(check.passed);assert.equal(check.handleBits,64);
  assert.ok(check.cachedReferences>=4);assert.equal(check.roundTrips,4);assert.ok(check.probeFreed&&check.weakProbe&&check.strongProbe);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  assert.ok(h.events.some(e=>e.event==='gc_cleanup'&&e.passed&&e.remaining===0));
  assert.ok(h.events.findIndex(e=>e.event==='gc_cleanup')<h.events.findIndex(e=>e.event==='stopped'));
});

test('encounter-only performs at most one encounter and never calls TryCapture',async()=>{
  const h=await harness({encounterOnly:true,diagnosticTrace:true});h.tick();h.tick();
  assert.deepEqual(h.calls,['encounter']);h.complete();h.tick();h.tick();
  assert.deepEqual(h.calls,['encounter']);assert.ok(h.detached);assert.equal(h.remainingHandles,0);
  assert.ok(h.events.some(e=>e.event==='ready'&&e.mode==='ENCOUNTER_ONLY'));
  assert.ok(h.events.some(e=>e.event==='probe_result'&&e.captureRequestsSent===0));
  const traces=h.events.filter(e=>e.event==='trace');assert.ok(traces.every(e=>e.tid===321));
  for(const phase of ['SendEncounterRequest:before','SendEncounterRequest:after','promise:reserved',
    'gc_target:before','gc_target:after','promise:error-flags:before','promise:error-flags:after',
    'promise:complete-flag:before','promise:complete-flag:after','promise:completed-value:before','promise:completed-value:after'])
    assert.ok(traces.some(e=>e.phase===phase),'Missing trace '+phase);
  assert.ok(traces.every((e,i)=>e.sequence===i+1));
});
test('rejected encounter-only does not retry another Pokémon or launch a ball',async()=>{
  const h=await harness({encounterOnly:true});h.tick();
  h.responses[0].p.fields.errorCalled=true;h.responses[0].p.fields.errorValue='Not available';h.tick();h.tick();
  assert.deepEqual(h.calls,['encounter']);assert.ok(h.detached);assert.equal(h.remainingHandles,0);
  assert.ok(h.events.some(e=>e.event==='probe_result'&&e.outcome==='rejected'));
});
test('detailed read-only traces never perform requests',async()=>{
  const h=await harness({readOnly:true,diagnosticTrace:true});for(let i=0;i<25;i++)h.tick(500);
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='trace'));
  assert.ok(h.events.some(e=>e.event==='read_stability'&&e.samples>=10&&e.elapsedMs>=9000&&e.cachedReferences>=4));
  h.command({type:'stop',reason:'Guard: PID unavailable'});
  assert.equal(h.events.find(e=>e.event==='stopped').reason,'Guard: PID unavailable');assert.equal(h.remainingHandles,0);
});
test('full engine with diagnostic tracing reaches TryCapture only after encounter response',async()=>{
  const h=await harness({diagnosticTrace:true});h.tick();h.tick();assert.deepEqual(h.calls,['encounter']);
  h.complete();h.tick();h.tick();assert.deepEqual(h.calls,['encounter','capture']);
  h.complete({Status:1});h.tick();h.command({type:'stop'});
  assert.ok(h.events.some(e=>e.event==='trace'&&e.phase==='TryCapture:before'));
  assert.ok(h.events.some(e=>e.event==='trace'&&e.phase==='TryCapture:after'));
  assert.equal(h.remainingHandles,0);assert.equal(h.events.find(e=>e.event==='stopped').caught,1);
});
test('encounter probe rejects spin, cleanup or disabled capture at initialization',async()=>{
  for(const flags of [{autoSpin:true},{cleanBag:true},{autoCatch:false}])
    await assert.rejects(harness({encounterOnly:true,...flags}),/Encounter probe requires catches only/);
});

test('full-width handles survive service cache, Pokémon and promise reuse',async()=>{
  const h=await harness();h.tick();h.complete();h.tick();h.tick();h.complete({Status:1});h.tick();
  assert.deepEqual(h.calls,['encounter','capture']);
  assert.ok(!h.events.some(e=>e.event==='error'));
  for(const call of h.gcCalls.filter(e=>e.name==='il2cpp_gchandle_get_target'||e.name==='il2cpp_gchandle_free'))
    assert.ok(BigInt(call.args[0])>0xffffffffn,'High bits must survive every native call');
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('a truncated native handle aborts before ready or game requests',async()=>{
  const h=await harness({gcNewReturnsNumber:true});h.tick();
  assert.match(h.events.find(e=>e.event==='error').message,/incompatible ABI/);
  assert.ok(!h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);
});

test('incorrect native target aborts preflight before requests',async()=>{
  const h=await harness({gcWrongTarget:true});h.tick();
  assert.match(h.events.find(e=>e.event==='error').message,/null reference/);
  assert.ok(!h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);assert.equal(h.remainingHandles,0);
});

test('failed native release is logged and never acknowledged as clean',async()=>{
  const h=await harness({readOnly:true});h.tick();
  assert.ok(h.events.some(e=>e.event==='ready'));
  // The harness changes its native API dynamically for shutdown, not probes.
  h.setFreeFailure();h.command({type:'stop'});
  assert.ok(h.events.some(e=>e.event==='gc_cleanup'&&!e.passed&&e.remaining>0));
  assert.ok(!h.events.find(e=>e.event==='stopped').cleanupPassed);
});

test('unknown Unity ABI stops before reference creation or game actions',async()=>{
  const h=await harness({unityVersion:'6000.4.0f1'});h.tick();
  assert.match(h.events.find(e=>e.event==='error').message,/GCHandle64 ABI/);
  assert.equal(h.gcCalls.length,0);assert.deepEqual(h.calls,[]);
});

test('boxed LatLng can also be unboxed without sending a read-only action',async()=>{
  const h=await harness({readOnly:true,locationMode:'boxed'});h.tick();
  assert.ok(h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);
});

test('missing and wrong location types report source, stage and stack before acting',async()=>{
  for(const locationMode of ['null','plain']) {
    const h=await harness({readOnly:true,locationMode});h.tick();
    const error=h.events.find(e=>e.event==='error');
    assert.ok(error);assert.equal(error.stage,'location:avatar');
    assert.match(error.message,/avatar: expected a Niantic.LatLng struct/);
    assert.match(error.stack,/readLocation/);assert.equal(error.stackTruncated,false);
    assert.ok(!h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);assert.ok(h.detached);
  }
});

test('a different native structure is rejected before actions',async()=>{
  const h=await harness({locationType:'Niantic.OtherStruct'});h.tick();
  assert.match(h.events.find(e=>e.event==='error').message,/incompatible struct/);
  assert.deepEqual(h.calls,[]);
});

test('nonfinite or out-of-range avatar coordinates stop before any request',async()=>{
  for(const playerLocation of [{Latitude:NaN},{Longitude:Infinity},{Latitude:91},{Longitude:-181}]) {
    const h=await harness({playerLocation});h.tick();
    assert.match(h.events.find(e=>e.event==='error').message,/coordinates non-finite or out of range/);
    assert.deepEqual(h.calls,[]);assert.ok(h.detached);
  }
});

test('Pokémon outside capture radius is skipped using struct coordinates',async()=>{
  const h=await harness({pokemonLocation:{Latitude:0.01}});h.tick();h.tick();
  assert.ok(h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);
});

test('Pokéstop outside spin radius is skipped using struct coordinates',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true,stopLocation:{Longitude:0.01}});h.tick();h.tick();
  assert.ok(h.events.some(e=>e.event==='ready'));assert.deepEqual(h.calls,[]);
});

test('spin request copies latitude and longitude from both native structs',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true,
    playerLocation:{Latitude:-12,Longitude:-77},stopLocation:{Latitude:-12.0001,Longitude:-77.0001}});
  h.tick();assert.deepEqual(h.calls,['spin']);
  const request=h.rpcRequests[0];assert.equal(request.id,101);
  assert.equal(request.fields.PlayerLatDegrees,-12);assert.equal(request.fields.PlayerLngDegrees,-77);
  assert.equal(request.fields.FortLatDegrees,-12.0001);assert.equal(request.fields.FortLngDegrees,-77.0001);
});

test('missing service identifies the requested service and emits an error trace',async()=>{
  const name='Niantic.Holoholo.IItemBag';
  const h=await harness({missingService:name});h.tick();
  const error=h.events.find(e=>e.event==='error');assert.ok(error);
  assert.equal(error.stage,'service:'+name);assert.match(error.message,/DiContainer.TryResolve/);
  assert.match(error.stack,/findService/);assert.deepEqual(h.calls,[]);assert.ok(h.detached);
});
test('one in-flight action and capture counted only on confirmed response',async()=>{
  const h=await harness();h.tick();h.tick();assert.deepEqual(h.calls,['encounter']);
  assert.equal(h.complete(),'encounter');h.tick();h.tick();assert.deepEqual(h.calls,['encounter','capture']);
  h.complete({Status:1});h.tick();h.tick(2200);
  assert.ok(h.events.some(e=>e.event==='stats'&&e.caught===1));assert.equal(h.calls.length,2);
});
test('escape retries are bounded, not counted as catches',async()=>{
  const h=await harness();h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:2});h.tick();h.tick();h.complete({Status:2});h.tick();h.tick();
  assert.equal(h.calls.filter(c=>c==='capture').length,2);
  assert.equal(h.events.filter(e=>e.event==='capture').at(-1).decision,'exhausted');
});
test('background, explicit stop and expired heartbeat stop new actions',async()=>{
  const h=await harness();h.tick(300,false);assert.deepEqual(h.calls,[]);
  h.tick();h.complete();h.command({type:'stop'});h.tick();
  assert.ok(h.detached);assert.deepEqual(h.calls,['encounter']);
  const second=await harness();second.watchdog();second.tick();assert.deepEqual(second.calls,[]);
  assert.ok(second.events.some(e=>e.event==='stopped'));
});
test('discard response waits for bag reconciliation, never blindly repeats',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true});h.tick();assert.deepEqual(h.calls,['discard']);
  h.complete({Result:1,NewCount:50});h.tick();h.tick(6000);assert.equal(h.calls.length,1);
  h.tick(10000);assert.ok(h.events.some(e=>e.event==='error'));assert.ok(h.detached);
});
test('spin success is confirmed and a stop cooldown prevents repeated requests',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true});h.tick();assert.deepEqual(h.calls,['spin']);
  h.complete({Result:1});h.tick();h.tick(6000);
  assert.equal(h.calls.length,1);assert.ok(h.events.some(e=>e.event==='stats'&&e.spun===1));
});
test('unconfirmed discard aborts, without incrementing deleted count',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true});h.tick();h.complete({Result:2,NewCount:100});h.tick();
  assert.ok(h.events.some(e=>e.event==='stopped'&&e.discarded===0));
});
test('unavailable encounter is skipped instead of looped or counted as captured',async()=>{
  const h=await harness();h.tick();h.responses[0].p.fields.errorCalled=true;
  h.tick();h.tick(2500);
  assert.equal(h.calls.length,1);assert.ok(!h.detached);
  assert.ok(h.events.some(e=>e.event==='encounter_rejected'));
});
test('inventory cleanup starts for selected excess even when bag is not full',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,bagFull:false});h.tick();h.tick(6000);
  assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Count,50);
  assert.ok(h.events.some(e=>e.event==='discard_plan'&&!e.bagFull&&e.cleanupTrigger==='EXCESS_FROM_START'&&e.startup));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('unsupported generic layout aborts during preflight, before any action',async()=>{
  const h=await harness({badLayout:true});h.tick();
  assert.deepEqual(h.calls,[]);assert.ok(h.detached);assert.ok(h.events.some(e=>e.event==='error'));
});

test('inspection preflight samples owned IDs without captures, spins, recycling or transfers',async()=>{
  const h=await harness({readOnly:true,inspectOwned:true,autoSpin:true,cleanBag:true});h.tick();
  assert.deepEqual(h.calls,[]);assert.deepEqual(h.ownedLookups,[h.ownedId]);
  const sample=h.events.find(e=>e.event==='pokemon_inspection');
  assert.equal(sample.origin,'sample');assert.equal(sample.id,h.ownedId);assert.equal(sample.decision,'retain');
  assert.equal(sample.hundo,true);assert.equal(sample.transferAllowed,false);
  assert.ok(h.events.some(e=>e.event==='inspection_preflight'&&e.passed&&e.sampled===1));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('a successful catch uses CapturedPokemonId, never the encounter ID, for inventory lookup',async()=>{
  const h=await harness({inspectOwned:true});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(600);
  assert.deepEqual(h.ownedLookups,[h.ownedId]);
  const read=h.events.find(e=>e.event==='pokemon_inspection'&&e.origin==='session');
  assert.equal(read.id,h.ownedId);assert.equal(read.encounterId,'111111111111111111');assert.equal(read.transferAllowed,false);
  assert.deepEqual(h.calls,['encounter','capture']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('unknown capture owned ID never looks up an encounter ID or transfers',async()=>{
  const h=await harness({inspectOwned:true});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=> '0'}});h.tick();h.tick(600);
  assert.deepEqual(h.ownedLookups,[]);assert.ok(h.events.some(e=>e.event==='pokemon_inspection'&&e.decision==='unknown'));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').caught,1);
});
test('missing inventory synchronization is read-only, bounded and reported as unknown',async()=>{
  const h=await harness({inspectOwned:true,ownedMissing:true});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(16000);
  assert.ok(h.events.some(e=>e.event==='pokemon_inspection'&&e.reasons?.includes('inventory_sync_timeout')));
  assert.deepEqual(h.calls,['encounter','capture']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('bag full without configured surpluses is diagnosed and does not delete arbitrary items',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,bagFull:true,discard:{Potion:200}});h.tick();h.tick(6000);
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='bag_cleanup_blocked'));
});
test('configured MaxPotion cleanup preserves the floor and reconciles before continuing',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,discard:{MaxPotion:200},inventory:{Potion:0,MaxPotion:296}});
  h.tick();assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Count,50);
  h.complete({Result:1,NewCount:246});h.tick();h.inventory.MaxPotion=246;h.tick();h.tick(1800);
  assert.equal(h.calls.length,2);
  assert.equal(h.rpcRequests[1].fields.Count,46);h.complete({Result:1,NewCount:200});h.tick();
  h.inventory.MaxPotion=200;h.tick(2000);h.tick(6000);assert.equal(h.calls.length,2);
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').discarded,96);
});
test('below-minimum recycle reply stops without another deletion',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true});h.tick();h.complete({Result:1,NewCount:49});h.tick();h.tick(20000);
  assert.equal(h.calls.length,1);assert.ok(h.detached);assert.match(h.events.find(e=>e.event==='error').message,/minimum/);
});
test('bag-full spin result applies a global backoff rather than spinning other stops immediately',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true});h.tick();h.complete({Result:4});h.tick();h.tick(6000);
  assert.equal(h.calls.length,1);assert.equal(h.events.find(e=>e.event==='bag_full').retryAfterMs,10000);
});
test('a previous full-bag reply cannot latch cleanup when bag space becomes available',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true,cleanBag:true,bagFull:false,discard:{Potion:200}});
  h.tick();h.complete({Result:4});h.tick();h.setBagFull(true);h.tick(6000);
  assert.ok(h.events.some(e=>e.event==='bag_cleanup_blocked'));h.setBagFull(false);h.tick(6000);h.tick(25000);
  assert.equal(h.calls.filter(c=>c==='spin').length,2);assert.equal(h.calls.filter(c=>c==='discard').length,0);
});

async function caughtForTransfer(overrides={}) {
  const h=await harness({transferEnabled:true,ownedFields:{individualAttack_:4,individualDefense_:8,individualStamina_:9},...overrides});
  h.tick();assert.equal(h.complete(),'encounter');h.tick();h.tick();assert.equal(h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}}),'capture');
  h.tick();return h;
}
test('transfer read-only validates protections and empty session scope with zero game requests',async()=>{
  const h=await harness({readOnly:true,transferEnabled:true,diagnosticTrace:true});h.tick();h.tick();
  assert.deepEqual(h.calls,[]);assert.ok(!h.events.some(e=>e.event==='error'));
  assert.ok(h.events.some(e=>e.event==='transfer_scope'&&e.queueEmpty&&!e.existingCollectionScan&&!e.queuePersisted&&e.sessionId==='mock_session_A'));
  assert.ok(h.events.some(e=>e.event==='transfer_validation'&&e.passed&&e.transferRequestsSent===0));
  assert.ok(h.events.some(e=>e.event==='transfer_sample'&&!e.transferAllowed));
  assert.ok(!h.events.some(e=>e.event==='transfer_request'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('active startup ignores old collection size and never accesses AllPokemon on PokemonBag',async()=>{
  const h=await harness({transferEnabled:true,collectionCount:999999,collectionAccessThrows:true});h.tick();
  assert.deepEqual(h.calls,['encounter']);assert.ok(h.events.some(e=>e.event==='ready'));
  assert.equal(h.collectionReads,0);assert.deepEqual(h.ownedLookups,[]);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('3907 existing Pokemon read-only samples stop at eight instead of blocking readiness',async()=>{
  const h=await harness({readOnly:true,transferEnabled:true,collectionLength:3907});h.tick();
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='ready'));
  assert.equal(h.collectionReads,1);assert.equal(h.collectionMoves,8);assert.equal(h.ownedLookups.length,8);
  assert.ok(h.events.some(e=>e.event==='transfer_validation'&&e.sampled===8));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('missing buddy/game master service aborts before sending even a capture',async()=>{
  for(const missingService of ['Niantic.Holoholo.Buddy.IBuddyPokemonService','IGameMasterData']) {
    const h=await harness({transferEnabled:true,missingService});h.tick();
    assert.deepEqual(h.calls,[]);assert.ok(h.detached);assert.equal(h.remainingHandles,0);
  }
});
test('old ordinary collection alone never queues transfers without a RootCatch capture',async()=>{
  const h=await harness({readOnly:true,transferEnabled:true,preexistingOwned:true,ownedFields:{individualAttack_:4}});h.tick();h.tick(20000);
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='transfer_sample'&&e.wouldTransfer));
  assert.ok(!h.events.some(e=>e.event==='owned_read_queued'||e.event==='capture_request'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('new ordinary capture transfers once: Success AND inventory absence required for counter',async()=>{
  const h=await caughtForTransfer();h.tick(600);
  assert.equal(h.calls.filter(c=>c==='transfer').length,1);
  assert.ok(h.events.some(e=>e.event==='transfer_decision'&&e.transferAllowed&&e.origin==='session'));
  const proof=h.events.find(e=>e.event==='capture_scope_result');assert.ok(proof.accepted&&proof.freshPromise);
  assert.equal(proof.requestId,h.events.find(e=>e.event==='capture_request').requestId);assert.equal(h.collectionReads,0);
  h.complete({Status:1});h.tick();h.tick(600);
  assert.ok(!h.events.some(e=>e.event==='transfer'&&e.confirmed));
  h.removeOwned();h.tick(600);h.tick(1000);
  assert.ok(h.events.some(e=>e.event==='transfer'&&e.confirmed&&e.inventoryAbsent));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').transferred,1);assert.equal(h.remainingHandles,0);
});
test('default retains newly captured shiny, hundo and shundo',async()=>{
  for(const flags of [{shiny:true},{ownedFields:{}},{shiny:true,ownedFields:{}}]) {
    const h=await caughtForTransfer(flags);h.tick(600);assert.ok(!h.calls.includes('transfer'));
    assert.ok(h.events.some(e=>e.event==='transfer_decision'&&e.decision==='retain'));h.command({type:'stop'});
  }
});
test('disabling shiny retention explicitly still honors perfect-IV retention',async()=>{
  const ordinary=await caughtForTransfer({shiny:true,keepShiny:false});ordinary.tick(600);assert.ok(ordinary.calls.includes('transfer'));
  ordinary.command({type:'stop'});
  const hundo=await caughtForTransfer({shiny:true,keepShiny:false,ownedFields:{}});hundo.tick(600);
  assert.ok(!hundo.calls.includes('transfer'));hundo.command({type:'stop'});
});
test('fresh favorite, buddy and invalid IV readings block release just before invocation',async()=>{
  for(const protection of ['favorite','buddy','iv']) {
    const h=await caughtForTransfer();
    if(protection==='favorite')h.ownedFields.favorite_=true;
    if(protection==='buddy')h.setBuddy(true);
    if(protection==='iv')h.ownedFields.individualAttack_=16;
    h.tick(600);assert.ok(!h.calls.includes('transfer'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  }
});
test('fusion, limited, deployed and special class captures are never released',async()=>{
  for(const flags of [{pokemonClass:1},{pokemonClass:2},{pokemonClass:3},
    {ownedFields:{individualAttack_:4,isFusion_:true}},
    {ownedFields:{individualAttack_:4,limitedPokemonIdentifier_:{content:'limited'}}},
    {ownedFields:{individualAttack_:4,deployedStationId_:{content:'station'}}}]) {
    const h=await caughtForTransfer(flags);h.tick(600);assert.ok(!h.calls.includes('transfer'));h.command({type:'stop'});
  }
});
test('server rejection preserves zero transfers, never retries the consumed ID',async()=>{
  for(const Status of [2,3,4,5,6,7]) {
    const h=await caughtForTransfer();h.tick(600);h.complete({Status});h.tick();h.tick(1000);
    assert.equal(h.calls.filter(c=>c==='transfer').length,1);assert.ok(!h.events.some(e=>e.event==='transfer'&&e.confirmed));
    h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').transferred,0);
  }
});
test('ambiguous transfer status and timeout stop without a retry or confirmed count',async()=>{
  for(const cause of ['unset','timeout','transport']) {
    const h=await caughtForTransfer();h.tick(600);
    if(cause==='unset')h.complete({Status:0});
    if(cause==='transport')h.responses[0].p.fields.errorCalled=true;
    h.tick(cause==='timeout'?19000:300);h.tick(30000);
    assert.ok(h.detached);assert.equal(h.calls.filter(c=>c==='transfer').length,1);
    assert.equal(h.events.find(e=>e.event==='stopped').transferred,0);assert.equal(h.remainingHandles,0);
  }
});
test('Success without inventory update times out, not counted or resent',async()=>{
  const h=await caughtForTransfer();h.tick(600);h.complete({Status:1});h.tick();h.tick(16000);
  assert.ok(h.detached);assert.equal(h.calls.filter(c=>c==='transfer').length,1);
  assert.equal(h.events.find(e=>e.event==='stopped').transferred,0);assert.equal(h.remainingHandles,0);
});
test('Stop or background before owned lookup cannot trigger a delayed transfer',async()=>{
  for(const cause of ['stop','background']) {
    const h=await caughtForTransfer();
    if(cause==='stop')h.command({type:'stop'});else h.tick(600,false);
    assert.ok(!h.calls.includes('transfer'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  }
});
test('Stop during transfer records uncertain outcome and releases all held references',async()=>{
  const h=await caughtForTransfer();h.tick(600);h.command({type:'stop'});h.tick(20000);
  assert.equal(h.calls.filter(c=>c==='transfer').length,1);assert.equal(h.remainingHandles,0);
  assert.ok(h.events.some(e=>e.event==='transfer_pending_at_stop'&&!e.retryAllowed));
  assert.equal(h.events.find(e=>e.event==='stopped').transferred,0);
});
test('transfer defaults OFF; no transfer path executes in ordinary inspection mode',async()=>{
  const h=await harness({inspectOwned:true,ownedFields:{individualAttack_:4}});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(600);
  assert.ok(!h.calls.includes('transfer'));assert.ok(!h.events.some(e=>e.event==='transfer_baseline'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('a favorite set BETWEEN initial inspection and final re-read is retained',async()=>{
  const h=await caughtForTransfer({onOwnedLookup(id,count,fields){if(count===2)fields.favorite_=true;}});
  h.tick(600);assert.ok(!h.calls.includes('transfer'));
  assert.ok(h.events.some(e=>e.event==='pokemon_inspection'&&e.decision==='review'));
  assert.ok(h.events.some(e=>e.event==='transfer_decision'&&e.reasons.includes('favorite')));h.command({type:'stop'});
});
test('after transfer limit, a second successful capture is kept and farming remains active',async()=>{
  const h=await caughtForTransfer({maxTransfers:1});h.tick(600);h.complete({Status:1});h.tick();h.removeOwned();h.tick(600);
  h.setOwnedId('18446744073709551613');h.tick(301000);assert.equal(h.complete(),'encounter');h.tick();h.tick();
  assert.equal(h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}}),'capture');h.tick();h.tick(600);
  assert.equal(h.calls.filter(c=>c==='transfer').length,1);assert.ok(!h.detached);
  assert.ok(h.events.some(e=>e.event==='transfer_limit'&&e.limit===1&&e.capturesContinue));
  h.command({type:'stop'});const final=h.events.find(e=>e.event==='stopped');assert.equal(final.caught,2);assert.equal(final.transferred,1);
});

test('omitted transfer quota defaults to unlimited across more than three confirmed transfers',async()=>{
  const h=await caughtForTransfer({maxTransfers:undefined});
  assert.equal(h.events.find(e=>e.event==='transfer_preflight').maxTransfers,0);
  for(let index=0;index<5;index++) {
    h.tick(600);assert.equal(h.complete({Status:1}),'transfer');h.tick();h.removeOwned();h.tick(600);
    if(index<4) {
      h.setOwnedId(String(18446744073709551610n+BigInt(index)));
      h.tick(301000);assert.equal(h.complete(),'encounter');h.tick();h.tick();
      assert.equal(h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}}),'capture');h.tick();
    }
  }
  assert.equal(h.calls.filter(x=>x==='transfer').length,5);
  assert.equal(h.collectionReads,0);
  assert.ok(!h.events.some(e=>e.event==='transfer_limit'));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').transferred,5);
});
test('invalid or absent capture-owned ID is not eligible for release',async()=>{
  const h=await harness({transferEnabled:true});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=> '0'}});h.tick();h.tick(20000);
  assert.ok(!h.calls.includes('transfer'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('already-completed capture promise is counted but never grants a transfer',async()=>{
  const h=await caughtForTransfer({precompletedCapture:true});h.tick(600);
  assert.ok(!h.calls.includes('transfer'));assert.deepEqual(h.ownedLookups,[]);
  assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&!e.accepted&&e.reason==='already_completed_or_failed_promise'));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').caught,1);assert.equal(h.remainingHandles,0);
});
test('unexpected native capture response class stops before inspecting or transferring',async()=>{
  const h=await harness({transferEnabled:true,captureResponseClass:'Result'});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(600);
  assert.ok(h.detached);assert.ok(!h.calls.includes('transfer'));assert.deepEqual(h.ownedLookups,[]);
  assert.equal(h.events.find(e=>e.event==='stopped').caught,0);assert.equal(h.remainingHandles,0);
});
test('late completed capture cannot authorize an irreversible release',async()=>{
  const h=await harness({transferEnabled:true});h.tick();h.complete();h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick(19000);h.tick(600);
  assert.ok(!h.calls.includes('transfer'));assert.deepEqual(h.ownedLookups,[]);
  assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&e.reason==='expired_or_invalid_response_time'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('a second capture response reusing an already handled owned ID is never released twice',async()=>{
  const h=await caughtForTransfer();h.tick(600);h.complete({Status:2});h.tick();h.tick(301000);
  assert.equal(h.complete(),'encounter');h.tick();h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(600);
  assert.equal(h.calls.filter(c=>c==='transfer').length,1);
  assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&e.reason==='duplicate_capture_owned_id'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('request spacing no longer adds a second delay after a completed encounter',async()=>{
  const h=await harness({actionDelayMs:300});h.tick();h.complete();h.tick(300);
  assert.deepEqual(h.calls,['encounter','capture']);
  assert.equal(h.events.find(e=>e.event==='encounter').time,h.events.find(e=>e.event==='pending'&&e.kind==='capture').time);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('a short response still respects the minimum request-to-request separation',async()=>{
  const h=await harness({actionDelayMs:1000});h.tick();h.complete();h.tick(300);
  assert.deepEqual(h.calls,['encounter']);h.tick(700);assert.deepEqual(h.calls,['encounter','capture']);
  const requests=h.events.filter(e=>e.event==='pending');assert.equal(requests[1].time-requests[0].time,1000);
  h.command({type:'stop'});
});
test('losing focus when encounter completes cannot dispatch the next capture',async()=>{
  const h=await harness();h.tick();h.complete();h.tick(300,false);
  assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('missing captured inventory entry no longer blocks another available encounter',async()=>{
  const h=await caughtForTransfer({ownedMissing:true});h.setEncounterId('222222222222222222');h.tick(300);
  assert.equal(h.calls.filter(x=>x==='encounter').length,2);assert.ok(!h.calls.includes('transfer'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('transfer Success can reconcile cooperatively while a new encounter is requested',async()=>{
  const h=await caughtForTransfer();h.tick(300);h.complete({Status:1});h.setEncounterId('222222222222222222');h.tick(300);
  assert.equal(h.calls.filter(x=>x==='encounter').length,2);assert.equal(h.calls.filter(x=>x==='transfer').length,1);
  assert.ok(!h.events.some(e=>e.event==='transfer'&&e.confirmed));
  h.command({type:'stop'});assert.ok(h.events.some(e=>e.event==='transfer_pending_at_stop'&&e.serverAck&&!e.retryAllowed));
  assert.equal(h.remainingHandles,0);
});
test('hot inventory path reads only three ball counts and explicitly configured types',async()=>{
  const h=await harness({readOnly:true});h.tick();const initial=h.itemCountReads;
  assert.ok(h.events.some(e=>e.event==='inventory_catalog'&&e.items.length===50));
  h.tick();assert.equal(h.itemCountReads-initial,4);h.command({type:'stop'});
});
test('all fourteen common types recycle by exact ID, preserving the chosen keep floor',async()=>{
  for(const entry of CATALOG.filter(x=>x.discardable&&!x.special)) {
    const h=await harness({autoCatch:false,cleanBag:true,discard:{[entry.name]:100},inventory:{[entry.name]:105},maxDiscardedItems:20});h.tick();
    assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Item,entry.id);assert.equal(h.rpcRequests[0].fields.Count,5);
    h.complete({Result:1,NewCount:100});h.tick();h.inventory[entry.name]=100;h.tick(2000);
    assert.equal(h.calls.length,1);h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').discarded,5);
  }
});
test('twenty-item total quota prevents a second deletion, including across types',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,discard:{Potion:0,Revive:0},inventory:{Potion:100,Revive:100},maxDiscardedItems:20});h.tick();
  assert.equal(h.rpcRequests[0].fields.Count,20);h.complete({Result:1,NewCount:80});h.tick();h.inventory.Potion=80;h.tick(2000);
  assert.equal(h.calls.length,1);assert.ok(h.events.some(e=>e.event==='discard_limit'&&e.maximum===20));h.command({type:'stop'});
});

test('omitted cleanup quota deletes surplus in bounded batches without a session total cap',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,discard:{Potion:50},inventory:{Potion:240},maxDiscardedItems:undefined});
  h.tick();assert.equal(h.events.find(e=>e.event==='inventory_catalog').maxDiscardedItems,0);
  for(const remaining of [190,140,90,50]) {
    const request=h.rpcRequests.at(-1);assert.equal(request.fields.Item,101);
    assert.equal(request.fields.Count,Math.min(h.inventory.Potion-50,50));
    assert.equal(h.complete({Result:1,NewCount:remaining}),'discard');h.tick();h.inventory.Potion=remaining;h.tick(2000);
  }
  assert.deepEqual(h.calls,['discard','discard','discard','discard']);
  assert.ok(!h.events.some(e=>e.event==='discard_limit'));
  assert.equal(h.inventory.Potion,50);
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').discarded,190);
});
test('selected golden/silver berry enum mismatch or absent field stops before deletion',async()=>{
  for(const flags of [{missingItemFields:['GoldenPinapBerry']},{itemIdOverrides:{GoldenPinapBerry:707}}]) {
    const h=await harness({autoCatch:false,cleanBag:true,discard:{GoldenPinapBerry:0},inventory:{GoldenPinapBerry:100},...flags});h.tick();
    assert.deepEqual(h.calls,[]);assert.ok(h.detached);assert.equal(h.remainingHandles,0);
  }
});
test('protected, unknown and malformed cleanup settings are rejected at initialization',async()=>{
  for(const name of ['MasterBall','WildBall','PremierBall','IncubatorBasic','BlukBerry','Unknown'])
    await assert.rejects(harness({discard:{[name]:0}}),/not allowed/);
  for(const maxDiscardedItems of [-1,0.5,10001])await assert.rejects(harness({maxDiscardedItems}),/Invalid item quota/);
});
test('all nineteen special types require explicit selection and preserve their selected floor',async()=>{
  for(const entry of CATALOG.filter(x=>x.special)) {
    const h=await harness({autoCatch:false,cleanBag:true,discard:{[entry.name]:10},inventory:{[entry.name]:12},maxDiscardedItems:5});h.tick();
    assert.equal(h.rpcRequests[0].fields.Item,entry.id);assert.equal(h.rpcRequests[0].fields.Count,2);
    h.complete({Result:1,NewCount:10});h.tick();h.inventory[entry.name]=10;h.tick(2000);
    assert.equal(h.calls.length,1);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  }
});
test('incompatible recycle response stops without counting or resending deletion',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,discardResponseClass:'Result'});h.tick();
  h.complete({Result:1,NewCount:50});h.tick();h.tick(20000);
  assert.ok(h.detached);assert.equal(h.calls.length,1);assert.equal(h.events.find(e=>e.event==='stopped').discarded,0);
});

test('item cleanup must reconcile before a queued Pokemon transfer or another write',async()=>{
  const h=await caughtForTransfer({cleanBag:true,bagFull:false,ownedMissing:true,discard:{Potion:100}});
  h.inventory.Potion=150;h.tick(1200);assert.equal(h.calls.filter(c=>c==='discard').length,1);
  assert.equal(h.complete({Result:1,NewCount:100}),'discard');h.tick(300);h.setOwnedMissing(false);h.tick(300);
  assert.ok(!h.calls.includes('transfer'));assert.equal(h.calls.filter(c=>c==='discard').length,1);
  h.inventory.Potion=100;h.tick(300);assert.equal(h.calls.filter(c=>c==='transfer').length,1);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('startup normalizes several selected types before any capture or spin',async()=>{
  const h=await harness({cleanBag:true,bagFull:false,autoSpin:true,discard:{Potion:100,RazzBerry:10},inventory:{Potion:105,RazzBerry:14}});
  h.tick();assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Count,5);
  h.complete({Result:1,NewCount:100});h.inventory.Potion=100;h.tick(300);h.tick(1800);
  assert.deepEqual(h.calls,['discard','discard']);assert.equal(h.rpcRequests[1].fields.Item,701);assert.equal(h.rpcRequests[1].fields.Count,4);
  h.complete({Result:1,NewCount:10});h.inventory.RazzBerry=10;h.tick(300);h.tick(1800);
  assert.deepEqual(h.calls,['discard','discard','spin']);
  assert.ok(h.events.some(e=>e.event==='inventory_startup_complete'&&e.reason==='within_limits'&&!e.surplusRemains));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('the same per-type limit cleans newly gained items after startup without a full bag',async()=>{
  const h=await harness({autoCatch:false,cleanBag:true,bagFull:false,discard:{RazzBerry:10},inventory:{RazzBerry:10}});
  h.tick();assert.deepEqual(h.calls,[]);h.inventory.RazzBerry=17;h.tick(1800);
  assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Item,701);assert.equal(h.rpcRequests[0].fields.Count,7);
  h.complete({Result:1,NewCount:10});h.inventory.RazzBerry=10;h.tick(1800);assert.equal(h.calls.length,1);
  h.command({type:'stop'});
});

test('startup safety quota ends normalization but leaves excess protected from further deletion',async()=>{
  const h=await harness({cleanBag:true,bagFull:false,maxDiscardedItems:5});h.tick();
  assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Count,5);
  h.complete({Result:1,NewCount:95});h.inventory.Potion=95;h.tick(300);h.tick(1800);
  assert.deepEqual(h.calls,['discard','encounter']);
  assert.ok(h.events.some(e=>e.event==='inventory_startup_complete'&&e.reason==='quota_reached'&&e.surplusRemains));
  assert.ok(h.events.some(e=>e.event==='discard_limit'&&e.maximum===5));h.command({type:'stop'});
});

test('below-limit inventory in a non-full bag never starves selected spins',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true,cleanBag:true,bagFull:false,discard:{Potion:100}});
  h.tick();assert.deepEqual(h.calls,['spin']);h.complete({Result:1});h.tick();h.tick(301000);
  assert.deepEqual(h.calls,['spin','spin']);assert.ok(!h.events.some(e=>e.event==='bag_cleanup_blocked'));h.command({type:'stop'});
});

test('read-only excess validation never begins startup cleanup or any mutation',async()=>{
  const h=await harness({readOnly:true,cleanBag:true,bagFull:false});h.tick();h.tick(2000);
  assert.deepEqual(h.calls,[]);assert.ok(!h.events.some(e=>['inventory_startup_complete','inventory_check','discard_plan'].includes(e.event)));
  assert.ok(h.events.some(e=>e.event==='inventory_catalog'&&e.cleanupTrigger==='EXCESS_FROM_START'));h.command({type:'stop'});
});

test('old or missing full-bag-only configuration cannot authorize cleanup from startup',async()=>{
  for(const cleanupTrigger of [undefined,'FULL_BAG_ONLY'])
    await assert.rejects(harness({cleanBag:true,cleanupTrigger}),/EXCESS_FROM_START confirmation/);
});

test('identity is read-only and an active proof for another player cannot send requests',async()=>{
  const read=await harness({readOnly:true});read.tick();
  assert.deepEqual(read.calls,[]);assert.equal(read.events.find(e=>e.event==='session_identity').playerId,'mock_player_A');
  read.command({type:'stop'});
  const wrong=await harness({expectedPlayerId:'mock_player_B'});wrong.tick();
  assert.deepEqual(wrong.calls,[]);assert.ok(wrong.events.some(e=>e.event==='session_changed'&&e.reason==='read_only_player_mismatch'));
  assert.equal(wrong.remainingHandles,0);
});

test('account change before a completed encounter prevents capture and all queued mutations',async()=>{
  const h=await harness({transferEnabled:true,expectedPlayerId:'mock_player_A'});h.tick();h.complete();h.setPlayerId('mock_player_B');h.tick(1000);
  assert.deepEqual(h.calls,['encounter']);assert.ok(h.events.some(e=>e.event==='session_changed'&&e.reason==='player_id_changed'));
  assert.equal(h.remainingHandles,0);
});

test('service replacement or removed scene stops even when the player ID is unchanged',async()=>{
  for(const change of ['replacePlayerService','replaceMapService','removeScene']) {
    const h=await harness({autoCatch:false,autoSpin:false});h.tick();h[change]();h.tick(1200);
    assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='session_changed'));assert.equal(h.remainingHandles,0);
  }
});

test('missing player identity stops preflight without granting any transfer or cleanup',async()=>{
  const h=await harness({playerId:'',cleanBag:true});h.tick();assert.deepEqual(h.calls,[]);
  assert.ok(h.events.some(e=>e.event==='error'&&e.message.includes('PlayerId')));assert.equal(h.remainingHandles,0);
});

test('full Pokemon storage pauses captures but allows spin and resumes when space is available',async()=>{
  const h=await harness({pokemonCount:1000,autoSpin:true});h.tick();assert.deepEqual(h.calls,['spin']);
  h.complete({Result:1});h.tick();assert.ok(!h.calls.includes('encounter'));
  assert.ok(h.events.some(e=>e.event==='capture_paused'&&e.reason==='pokemon_storage_full'&&e.preexistingPokemonPreserved));
  assert.equal(h.collectionReads,0);h.setPokemonCount(999);h.tick(1000);
  assert.ok(h.calls.includes('encounter'));h.command({type:'stop'});
});

test('zero balls does not repeatedly open encounters or prevent spinning',async()=>{
  const h=await harness({autoSpin:true,inventory:{PokeBall:0,GreatBall:0,UltraBall:0}});h.tick();h.complete({Result:1});h.tick();
  assert.deepEqual(h.calls,['spin']);assert.ok(h.events.some(e=>e.event==='capture_paused'&&e.reason==='no_balls'));
  h.command({type:'stop'});
});

test('full item bag pauses futile spins and resumes when capacity becomes available',async()=>{
  const h=await harness({autoCatch:false,autoSpin:true,cleanBag:true,bagFull:true,discard:{Potion:100}});
  h.tick();assert.deepEqual(h.calls,[]);
  for(let i=0;i<35;i++)h.tick(1000);
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='spin_paused'&&e.reason==='item_bag_full'));
  h.setBagFull(false);h.tick(6000);assert.deepEqual(h.calls,['spin']);
  assert.ok(h.events.some(e=>e.event==='spin_resumed'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('eight simulated hours stay bounded and emit health without any hidden session deadline',async()=>{
  const h=await harness({autoCatch:false,diagnosticTrace:true});h.tick();
  for(let i=0;i<480;i++)h.tick(60000);
  assert.ok(!h.detached);assert.deepEqual(h.calls,[]);
  assert.ok(h.events.filter(e=>e.event==='health').length>=480);assert.ok(h.remainingHandles<12);
  assert.ok(h.events.filter(e=>e.event==='trace_summary').length>400);h.command({type:'stop'});
  assert.equal(h.remainingHandles,0);
});

test('300 own captures/transfers reconcile with bounded handles and expiring encounter cache',async()=>{
  const h=await harness({transferEnabled:true,maxTransfers:0,expectedPlayerId:'mock_player_A',
    ownedFields:{individualAttack_:1,individualDefense_:1,individualStamina_:1},diagnosticTrace:true});
  for(let i=0;i<300;i++) {
    h.setEncounterId(String(100000+i));h.setOwnedId(String(200000+i));h.tick(60000);
    assert.equal(h.complete(),'encounter');h.tick();
    assert.equal(h.complete({Status:1,CapturedPokemonId:{toString:()=>String(200000+i)}}),'capture');h.tick();h.tick(300);
    assert.equal(h.complete({Status:1}),'transfer');h.removeOwned();h.tick(300);h.tick(300);
    assert.ok(h.remainingHandles<15,'GC references grow at capture '+i);
    assert.ok(!h.detached);
  }
  const health=h.events.filter(e=>e.event==='health').at(-1);
  assert.ok(health.seenCache<=6);assert.equal(h.events.filter(e=>e.event==='transfer'&&e.confirmed).length,300);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('a full ITEM bag must never pause captures when Pokemon storage has space',async()=>{
  const h=await harness({itemBagFull:true,totalItems:1250,maxItems:1250,pokemonCount:120,maxPokemon:450});h.tick();
  assert.deepEqual(h.calls,['encounter']);h.complete();h.tick();assert.deepEqual(h.calls,['encounter','capture']);
  const health=h.events.find(e=>e.event==='health');
  assert.equal(health.itemBagFull,true);assert.equal(health.pokemonBagFull,false);
  assert.equal(health.pokemonCount,120);assert.equal(health.maxPokemon,450);assert.equal(h.collectionReads,0);
  assert.ok(!h.events.some(e=>e.event==='capture_paused'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('Pokemon fullness is independent of item bag and resumes automatically after room is available',async()=>{
  const h=await harness({itemBagFull:false,pokemonCount:450,maxPokemon:450});h.tick();
  assert.deepEqual(h.calls,[]);assert.equal(h.events.find(e=>e.event==='capture_paused').pokemonCount,450);
  h.setPokemonCount(449);h.tick(600);assert.deepEqual(h.calls,['encounter']);
  assert.ok(h.events.some(e=>e.event==='capture_resumed'&&e.previousReason==='pokemon_storage_full'));
  assert.equal(h.collectionReads,0);h.command({type:'stop'});
});

test('invalid Pokemon count/capacity aborts before any mutation rather than falling back to item fullness',async()=>{
  for(const values of [{pokemonCount:-1},{pokemonCount:NaN},{pokemonCount:1.5},{maxPokemon:0},{maxPokemon:NaN}]) {
    const h=await harness({...values,autoSpin:true,cleanBag:true});h.tick();
    assert.deepEqual(h.calls,[]);assert.ok(h.detached);assert.equal(h.remainingHandles,0);
  }
});

test('nearest live eligible Pokemon is chosen instead of the first dictionary entry',async()=>{
  const h=await harness({mapPokemon:[{id:'10',location:{Latitude:0.00030}},{id:'11',location:{Latitude:0.00005}},
    {id:'12',destroyed:true},{id:'13',className:'QuestMapPokemon'},{id:'14',location:{Latitude:0.001}}]});h.tick();
  assert.equal(h.events.find(e=>e.event==='pending').id,'11');
  const scan=h.events.find(e=>e.event==='map_scan');assert.equal(scan.eligible,2);
  assert.equal(scan.destroyed,1);assert.equal(scan.unsupported,1);assert.equal(scan.outOfRange,1);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('walking out of encounter range skips it and opens the next nearby Pokemon without a throw',async()=>{
  const h=await harness({mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0005}}]});h.tick();h.complete();
  h.setPlayerLocation({Latitude:0.0005});h.tick();
  assert.deepEqual(h.calls,['encounter','encounter']);
  assert.equal(h.events.filter(e=>e.event==='pending').at(-1).id,'11');
  assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.id==='10'&&e.reason==='out_of_range'&&e.captureRequestsSent===0));
  assert.ok(!h.detached);h.complete();h.tick();assert.equal(h.calls.at(-1),'capture');
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('an escape does not trigger another throw if the route has already left that Pokemon',async()=>{
  const h=await harness({mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0005}}]});h.tick();h.complete();h.tick();
  h.complete({Status:2});h.setPlayerLocation({Latitude:0.0005});h.tick();
  assert.deepEqual(h.calls,['encounter','capture','encounter']);
  assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='out_of_range'&&e.captureRequestsSent===1));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('a despawn after encounter clears its GC target and allows the next live entry',async()=>{
  const first={id:'10'},second={id:'11',location:{Latitude:0.0001}};
  const h=await harness({mapPokemon:[first,second]});h.tick();h.complete();first.destroyed=true;h.tick();
  assert.deepEqual(h.calls,['encounter','encounter']);assert.equal(h.events.filter(e=>e.event==='pending').at(-1).id,'11');
  assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='destroyed'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('a completed rejection does not impose an extra second before selecting a different nearby target',async()=>{
  const h=await harness({mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0001}}]});h.tick();
  h.responses[0].p.fields.errorCalled=true;h.tick(300);
  assert.deepEqual(h.calls,['encounter','encounter']);assert.equal(h.events.filter(e=>e.event==='pending').at(-1).id,'11');
  assert.equal(h.responses.filter(r=>!r.p.fields.errorCalled&&!r.p.fields.completeCalled).length,1);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('known unsuccessful capture responses are skipped, not counted or granted transfer scope',async()=>{
  for(const status of [0,5]) {
    const h=await harness({transferEnabled:true,mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0001}}]});
    h.tick();h.complete();h.tick();h.complete({Status:status});h.tick();
    assert.deepEqual(h.calls,['encounter','capture','encounter']);assert.ok(!h.detached);
    assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&!e.accepted&&e.reason==='capture_not_successful'));
    assert.ok(!h.calls.includes('transfer'));h.command({type:'stop'});
    assert.equal(h.events.find(e=>e.event==='stopped').caught,0);assert.equal(h.remainingHandles,0);
  }
});

test('missed throws have the same bounded retry policy as escapes, then scanning continues',async()=>{
  const h=await harness({mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0001}}]});h.tick();h.complete();h.tick();
  h.complete({Status:4});h.tick();h.complete({Status:4});h.tick();
  assert.deepEqual(h.calls,['encounter','capture','capture','encounter']);
  assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='capture_exhausted'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('wrong encounter class or unknown capture status still stops instead of guessing',async()=>{
  const wrong=await harness({encounterResponseClass:'Unknown.Response'});wrong.tick();wrong.complete();wrong.tick();
  assert.deepEqual(wrong.calls,['encounter']);assert.ok(wrong.detached);assert.equal(wrong.remainingHandles,0);
  const unknown=await harness();unknown.tick();unknown.complete();unknown.tick();unknown.complete({Status:99});unknown.tick();
  assert.deepEqual(unknown.calls,['encounter','capture']);assert.ok(unknown.detached);assert.equal(unknown.remainingHandles,0);
});

test('a moving avatar never authorizes overlapping requests while a capture outcome is still unknown',async()=>{
  const h=await harness({mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0005}}]});h.tick();h.complete();h.tick();
  h.setPlayerLocation({Latitude:0.0005});h.tick(1000);h.tick(1000);
  assert.deepEqual(h.calls,['encounter','capture']);h.tick(18000);
  assert.ok(h.detached);assert.deepEqual(h.calls,['encounter','capture']);assert.equal(h.remainingHandles,0);
});

async function automaticHarness(overrides={}) {
  const h=await harness({automaticPreflight:true,...overrides});h.tick();
  for(let i=0;i<20;i++)h.tick(500);
  return h;
}
test('automatic preflight stays read-only on the same attach, then activates only with its bound permit',async()=>{
  const h=await automaticHarness({cleanBag:true,transferEnabled:true});
  assert.deepEqual(h.calls,[]);const proof=h.events.find(e=>e.event==='preflight_validated');assert.ok(proof);
  assert.equal(proof.gameRequestsSent,0);assert.equal(h.events.filter(e=>e.event==='attached').length,1);
  h.command({type:'activate',...proof});h.tick();assert.deepEqual(h.calls,[]);
  assert.ok(h.events.some(e=>e.event==='ready'&&e.mode==='INTERNAL'&&e.automaticPreflightPassed));
  h.tick();assert.deepEqual(h.calls,['discard']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('manual read-only cannot be activated by a farm command',async()=>{
  const h=await harness({readOnly:true});h.tick();h.command({type:'activate',sessionId:'mock_session_A'});h.tick(1000);
  assert.deepEqual(h.calls,[]);assert.ok(h.events.some(e=>e.event==='activation_rejected'));h.command({type:'stop'});
});
test('early or stale automatic activation never causes writes',async()=>{
  const early=await harness({automaticPreflight:true});early.tick();early.command({type:'activate'});early.tick();
  assert.deepEqual(early.calls,[]);early.command({type:'stop'});
  const h=await automaticHarness();const proof=h.events.find(e=>e.event==='preflight_validated');h.tick(6000);
  h.command({type:'activate',...proof});h.tick();assert.deepEqual(h.calls,[]);h.command({type:'stop'});
});
test('account changes or Stop during automatic preflight do not grant captures or transfers',async()=>{
  const changed=await automaticHarness({transferEnabled:true});const proof=changed.events.find(e=>e.event==='preflight_validated');
  changed.setPlayerId('player_B');changed.command({type:'activate',...proof});changed.tick();assert.deepEqual(changed.calls,[]);
  assert.equal(changed.remainingHandles,0);
  const stopped=await automaticHarness({transferEnabled:true});stopped.command({type:'stop'});stopped.tick();
  assert.deepEqual(stopped.calls,[]);assert.equal(stopped.remainingHandles,0);
});
test('losing game focus invalidates automatic startup and demands a new full window',async()=>{
  const h=await automaticHarness();const proof=h.events.find(e=>e.event==='preflight_validated');h.tick(300,false);
  h.command({type:'activate',...proof});h.tick();assert.deepEqual(h.calls,[]);
  for(let i=0;i<20;i++)h.tick(500);
  const next=h.events.filter(e=>e.event==='preflight_validated').at(-1);assert.notEqual(proof.token,next.token);
  h.command({type:'activate',...next});h.tick();h.tick();assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});
});

test('UGPhone native location starts before map validation, retains the read-only gate, and stops cleanly',async()=>{
  const h=await automaticHarness({fakeLocationLifecycle:true,nativeLocationJNI:true,edition:'ugphone'});
  assert.deepEqual(h.calls,[]);
  assert.ok(h.events.findIndex(e=>e.event==='native_location_ready')<h.events.findIndex(e=>e.event==='observing'));
  const proof=h.events.find(e=>e.event==='preflight_validated');h.command({type:'activate',...proof});h.tick();h.tick();
  assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});
  assert.ok(h.events.some(e=>e.event==='native_location_cleanup'&&e.passed));assert.equal(h.remainingHandles,0);
});
test('native location failure does not initialize game services or send farming requests',async()=>{
  const h=await harness({fakeLocationLifecycle:true,nativeLocationJNI:true,edition:'ugphone',automaticPreflight:true,locationInstallFailure:true});
  assert.deepEqual(h.calls,[]);assert.equal(h.gcCalls.length,0);
  assert.ok(h.events.some(e=>e.event==='error'&&e.stage==='setup:native-location'));
  assert.ok(h.events.some(e=>e.event==='stopped'));assert.ok(!h.events.some(e=>e.event==='observing'));
});
test('native JNI cleanup failure cannot acknowledge successful engine cleanup',async()=>{
  const h=await harness({fakeLocationLifecycle:true,nativeLocationJNI:true,edition:'ugphone',automaticPreflight:true,locationCleanupFailure:true});
  h.tick();h.command({type:'stop'});
  assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,false);
  assert.ok(h.events.some(e=>e.event==='native_location_cleanup'&&!e.passed));
});
test('native location is rejected outside the explicit UGPhone automatic profile',async()=>{
  await assert.rejects(harness({nativeLocationJNI:true,edition:'default',automaticPreflight:true}),/restricted/);
  await assert.rejects(harness({nativeLocationJNI:true,edition:'ugphone',readOnly:true}),/restricted/);
});

test('unlimited throws survive thirty settled retries and only count confirmed success',async()=>{
  const h=await harness({maxThrows:0,actionDelayMs:100,transferEnabled:true});h.tick();h.complete();h.tick(100);
  for(let i=0;i<30;i++) {
    assert.equal(h.responses.filter(r=>!r.p.fields.completeCalled&&!r.p.fields.errorCalled).length,1);
    h.complete({Status:i%2?2:4});h.tick(100);
  }
  assert.equal(h.calls.filter(c=>c==='capture').length,31);assert.ok(!h.detached);
  assert.ok(!h.events.some(e=>e.event==='target_skipped'&&e.reason==='capture_exhausted'));
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick(100);
  assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&e.accepted));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').caught,1);assert.equal(h.remainingHandles,0);
});

test('unlimited retries still skip a target when a 25 km/h route leaves it behind',async()=>{
  const h=await harness({maxThrows:0,actionDelayMs:100,mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.00045}}]});
  h.tick();h.complete();h.tick(100);h.complete({Status:2});h.tick(100);
  h.setPlayerLocation({Latitude:0.00045});h.tick(7200);
  assert.deepEqual(h.calls,['encounter','capture','capture']);
  h.complete({Status:2});h.tick(100);
  assert.deepEqual(h.calls,['encounter','capture','capture','encounter']);
  assert.equal(h.events.filter(e=>e.event==='pending').at(-1).id,'11');
  assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='out_of_range'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('a despawn after repeated unlimited throws releases the target and selects another',async()=>{
  const first={id:'10'},second={id:'11',location:{Latitude:0.0001}};
  const h=await harness({maxThrows:0,mapPokemon:[first,second]});h.tick();h.complete();h.tick();
  for(let i=0;i<6;i++){h.complete({Status:2});h.tick();}
  first.destroyed=true;h.complete({Status:2});h.tick();
  assert.equal(h.calls.filter(c=>c==='capture').length,7);assert.equal(h.calls.at(-1),'encounter');
  assert.equal(h.events.filter(e=>e.event==='pending').at(-1).id,'11');
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('settled capture rejection consumes its grant request without stopping the other targets',async()=>{
  for(const flag of ['errorCalled','statusErrorCalled']) {
    const h=await harness({maxThrows:0,transferEnabled:true,mapPokemon:[{id:'10'},{id:'11',location:{Latitude:0.0001}}]});
    h.tick();h.complete();h.tick();const rejected=h.responses.shift();
    rejected.p.fields[flag]=true;rejected.p.fields.errorValue='Target unavailable';h.tick();
    assert.deepEqual(h.calls,['encounter','capture','encounter']);assert.ok(!h.detached);
    assert.ok(h.events.some(e=>e.event==='capture_rejected'&&e.id==='10'&&!e.transferAllowed));
    h.complete();h.tick();assert.equal(h.calls.at(-1),'capture');
    h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();
    assert.ok(h.events.some(e=>e.event==='capture_scope_result'&&e.encounterId==='11'&&e.accepted));
    h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').caught,1);assert.equal(h.remainingHandles,0);
  }
});

test('a full item bag neither sends a spin request nor blocks captures',async()=>{
  const h=await harness({bagFull:true,autoSpin:true,maxThrows:0});h.tick();
  assert.deepEqual(h.calls,['encounter']);assert.ok(h.events.some(e=>e.event==='spin_paused'));
  h.complete();h.tick();assert.deepEqual(h.calls,['encounter','capture']);
  assert.ok(!h.calls.includes('spin'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('map logs expose all visible distances and measured movement without new map RPCs',async()=>{
  const h=await harness({mapPokemon:[{id:'10',location:{Latitude:0.001}},{id:'11',location:{Latitude:0.002}}]});h.tick();
  const first=h.events.find(e=>e.event==='map_scan');assert.equal(first.eligible,0);
  assert.ok(first.closestVisibleMeters>100);assert.equal(first.visibleBeyond80,2);
  h.setPlayerLocation({Latitude:0.000312267});h.tick(5000);
  const second=h.events.filter(e=>e.event==='map_scan').at(-1);
  assert.ok(Math.abs(second.observedSpeedKmh-25)<0.01);assert.equal(second.mapIdsUnchangedMs,5000);
  assert.deepEqual(h.calls,[]);assert.deepEqual(h.rpcRequests,[]);h.command({type:'stop'});
});

test('requested grades change actual throw struct fields, not only the displayed setting',async()=>{
  for(const [quality,reticle] of [['NORMAL',1.1],['NICE',0.85],['GREAT',0.5],['EXCELLENT',0.05]]) {
    const h=await harness({throwQuality:quality});h.tick();h.complete();h.tick();
    assert.deepEqual(h.throwData,[{Missed:false,HitBullseye:true,Spinning:true,ReticleSize:reticle}]);
    assert.ok(h.events.some(e=>e.event==='throw_requested'&&e.quality===quality&&e.reticleSize===reticle));
    h.complete({Status:1});h.tick();
    assert.ok(h.events.some(e=>e.event==='capture'&&e.requestedThrowQuality===quality));
    h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  }
});

test('Excellent retries keep the selected reticle and do not imply successful captures',async()=>{
  const h=await harness({throwQuality:'EXCELLENT',maxThrows:0});h.tick();h.complete();h.tick();
  h.complete({Status:2});h.tick();h.complete({Status:3});h.tick();
  assert.equal(h.throwData.length,2);assert.ok(h.throwData.every(data=>data.ReticleSize===0.05));
  h.command({type:'stop'});const final=h.events.find(e=>e.event==='stopped');
  assert.equal(final.caught,0);assert.equal(final.fled,1);assert.equal(h.remainingHandles,0);
});

test('invalid grade settings are rejected before native initialization or game actions',async()=>{
  for(const throwQuality of ['INVALID','excellent',4])
    await assert.rejects(harness({throwQuality}),/Invalid throw quality/);
});

const shadeParams={edition:'ugphone',controlRecovery:true,notificationShadeSupport:true};
async function activatedShadeHarness() {
  const h=await automaticHarness(shadeParams);
  const proof=h.events.find(e=>e.event==='preflight_validated');assert.ok(proof);
  h.command({type:'activate',...proof});h.tick();return h;
}

test('validated shade proof allows captures after automatic preflight despite Java focus loss',async()=>{
  const h=await activatedShadeHarness();h.shadeTick(500);assert.deepEqual(h.calls,['encounter']);
  h.complete();h.shadeTick(500);assert.deepEqual(h.calls,['encounter','capture']);
  h.complete({Status:1});h.shadeTick(500);
  assert.ok(h.events.some(e=>e.event==='notification_shade'&&e.allowed));
  h.command({type:'stop'});assert.equal(h.events.find(e=>e.event==='stopped').caught,1);assert.equal(h.remainingHandles,0);
});

test('shade proof never replaces the visible read-only startup validation',async()=>{
  const h=await harness({automaticPreflight:true,...shadeParams});
  for(let i=0;i<30;i++)h.shadeTick(500);
  assert.deepEqual(h.calls,[]);assert.ok(!h.events.some(e=>e.event==='preflight_validated'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('another app or locked-screen negative proof revokes shade authorization immediately',async()=>{
  const h=await activatedShadeHarness();h.shadeTick(500);h.complete();
  h.tick(500,false,false,true);assert.deepEqual(h.calls,['encounter']);
  assert.ok(h.events.some(e=>e.event==='notification_shade'&&!e.allowed));
  h.tick(500,false,true,false);assert.deepEqual(h.calls,['encounter']);
  h.tick(500,true);assert.deepEqual(h.calls,['encounter','capture']);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('legacy callers cannot opt into a shade bypass without the sequenced UGPhone startup policy',async()=>{
  const h=await harness({notificationShadeSupport:true});h.tick();h.complete();h.shadeTick(500);
  assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('shade authorization expires with controller loss and Stop remains effective in the shade',async()=>{
  const h=await activatedShadeHarness();h.shadeTick(500);h.complete();
  h.tickWithoutHeartbeat(3100);assert.deepEqual(h.calls,['encounter']);
  h.shadeTick(500);assert.deepEqual(h.calls,['encounter','capture']);
  h.command({type:'stop'});assert.ok(h.detached);assert.equal(h.remainingHandles,0);
});

const berryParams={useBerries:true,berryOrder:['RazzBerry','PinapBerry','NanabBerry'],inventory:{RazzBerry:3,PinapBerry:4}};
async function berryHarness(extra={}) {
  const h=await harness({...berryParams,...extra});h.tick();h.complete();h.tick();return h;
}
test('berries use the actual ItemBag facade and wait for its response before the throw',async()=>{
  const h=await berryHarness();assert.deepEqual(h.calls,['encounter','berry']);
  assert.deepEqual(h.berryRequests,[{item:701,id:'111111111111111111'}]);
  assert.equal(h.throwData.length,0);h.tick();assert.equal(h.berryRequests.length,1);
  h.complete({Status:0,ActiveItem:701});h.tick();assert.deepEqual(h.calls,['encounter','berry','capture']);
  assert.equal(h.rpcRequests.length,0);
  const result=h.events.find(e=>e.event==='berry_result');assert.equal(result.elapsedMs,600);assert.equal(result.confirmed,true);
  h.complete({Status:1});h.tick();h.command({type:'stop'});
  assert.equal(h.events.find(e=>e.event==='stopped').berriesUsed,1);assert.equal(h.remainingHandles,0);
});
test('berry replies add no second interval after a response that already exceeded the request interval',async()=>{
  const h=await berryHarness({actionDelayMs:100});h.complete({Status:0,ActiveItem:701});h.tick(800);
  assert.deepEqual(h.calls,['encounter','berry','capture']);h.command({type:'stop'});
});
test('stock selection uses only the selected priority and requires no discarded-item setting',async()=>{
  const h=await berryHarness({berryOrder:['PinapBerry','RazzBerry'],discard:{}});
  assert.equal(h.berryRequests[0].item,705);h.command({type:'stop'});
});
test('empty stock skips berries without an added RPC or delay',async()=>{
  const h=await berryHarness({inventory:{RazzBerry:0,PinapBerry:0,NanabBerry:0,GoldenRazzBerry:10}});
  assert.deepEqual(h.calls,['encounter','capture']);h.command({type:'stop'});
});
test('an encounter active-item response avoids feeding another berry',async()=>{
  const h=await harness(berryParams);h.tick();h.complete({InitialActiveItem:708});h.tick();
  assert.deepEqual(h.calls,['encounter','capture']);h.command({type:'stop'});
});
test('a breakout reapplies a berry whereas a missed throw keeps the previous effect',async()=>{
  const h=await berryHarness({maxThrows:0});h.complete({Status:0,ActiveItem:701});h.tick();
  h.complete({Status:4});h.tick();assert.deepEqual(h.calls,['encounter','berry','capture','capture']);
  h.complete({Status:2});h.tick();assert.deepEqual(h.calls,['encounter','berry','capture','capture','berry']);
  h.complete({Status:0,ActiveItem:701});h.tick();assert.equal(h.calls.at(-1),'capture');
  h.complete({Status:3});h.tick();h.command({type:'stop'});
  assert.equal(h.events.find(e=>e.event==='stopped').berriesUsed,2);assert.equal(h.remainingHandles,0);
});
test('active-item-exists and no-stock responses still proceed to capture without false consumption',async()=>{
  for(const [Status,ActiveItem] of [[2,708],[3,0],[4,0],[7,0],[0,0]]) {
    const h=await berryHarness();h.complete({Status,ActiveItem});h.tick();
    assert.equal(h.calls.at(-1),'capture');h.command({type:'stop'});
    assert.equal(h.events.find(e=>e.event==='stopped').berriesUsed,0);assert.equal(h.remainingHandles,0);
  }
});
test('already-completed encounters move on without throwing a ball',async()=>{
  const h=await berryHarness();h.complete({Status:1,ActiveItem:0});h.tick();
  assert.deepEqual(h.calls,['encounter','berry']);assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='berry_encounter_completed'));
  h.command({type:'stop'});
});
test('a rejected berry promise does not break farming or endlessly repeat the berry',async()=>{
  const h=await berryHarness({maxThrows:0});const rejected=h.responses.shift();rejected.p.fields.errorCalled=true;
  rejected.p.fields.errorValue='network failure';h.tick();assert.equal(h.calls.at(-1),'capture');
  h.complete({Status:2});h.tick();assert.deepEqual(h.calls,['encounter','berry','capture','capture']);
  assert.ok(h.events.some(e=>e.event==='berry_rejected'&&e.retryAllowed===false));
  assert.ok(!h.events.some(e=>e.event==='error'));h.command({type:'stop'});
});
test('a target that left range during the berry reply is not thrown at',async()=>{
  const h=await berryHarness();h.setPlayerLocation({Latitude:0.001});h.complete({Status:0,ActiveItem:701});h.tick();
  assert.deepEqual(h.calls,['encounter','berry']);assert.ok(h.events.some(e=>e.event==='target_skipped'&&e.reason==='out_of_range'));
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});
test('a destroyed target during the berry reply is skipped without a capture',async()=>{
  const h=await berryHarness();h.setPokemonDestroyed(true);h.complete({Status:0,ActiveItem:701});h.tick();
  assert.equal(h.throwData.length,0);h.command({type:'stop'});
});
test('focus loss while a berry is pending prevents the subsequent ball until focus returns',async()=>{
  const h=await berryHarness();h.complete({Status:0,ActiveItem:701});h.tick(500,false);
  assert.deepEqual(h.calls,['encounter','berry']);h.tick(500,true);assert.equal(h.calls.at(-1),'capture');
  h.command({type:'stop'});
});
test('Stop releases full-width berry promise and target references without sending a ball',async()=>{
  const h=await berryHarness();h.command({type:'stop'});assert.deepEqual(h.calls,['encounter','berry']);
  assert.equal(h.remainingHandles,0);assert.ok(h.events.find(e=>e.event==='gc_cleanup').passed);
});
test('unavailable or incompatible berry facade disables only feeding before sending a request',async()=>{
  for(const extra of [{missingBerryMethod:true},{badBerrySignature:true}]) {
    const h=await berryHarness(extra);assert.deepEqual(h.calls,['encounter','capture']);
    assert.ok(h.events.some(e=>e.event==='berry_profile'&&e.available===false));
    assert.ok(!h.events.some(e=>e.event==='error'));h.command({type:'stop'});
  }
});
test('native berry failures and wrong response classes are not hidden as ordinary rejection',async()=>{
  const failed=await berryHarness({berryCallThrows:true});assert.ok(failed.events.some(e=>e.event==='error'&&/native berry/.test(e.message)));
  const wrong=await berryHarness({berryResponseClass:'WrongResult'});wrong.complete({Status:0,ActiveItem:701});wrong.tick();
  assert.ok(wrong.events.some(e=>e.event==='error'&&/berry response class/.test(e.message)));assert.equal(wrong.throwData.length,0);
});
test('read-only and encounter-only modes do not send berries',async()=>{
  const read=await harness({...berryParams,readOnly:true});read.tick();assert.deepEqual(read.calls,[]);read.command({type:'stop'});
  const probe=await harness({...berryParams,encounterOnly:true});probe.tick();probe.complete();probe.tick();
  assert.deepEqual(probe.calls,['encounter']);assert.equal(probe.remainingHandles,0);
});
test('turning berries off preserves the old capture sequence even with berries in stock',async()=>{
  const h=await berryHarness({useBerries:false});assert.deepEqual(h.calls,['encounter','capture']);h.command({type:'stop'});
});
test('cleanup keeps its configured surplus deletion before using the remaining berries',async()=>{
  const h=await harness({...berryParams,cleanBag:true,discard:{RazzBerry:2},inventory:{RazzBerry:7}});h.tick();
  assert.deepEqual(h.calls,['discard']);assert.equal(h.rpcRequests[0].fields.Item,701);assert.equal(h.rpcRequests[0].fields.Count,5);
  h.inventory.RazzBerry=2;h.complete({Result:1,NewCount:2});h.tick(2000);h.tick(1600);
  assert.equal(h.calls.at(-1),'encounter');h.complete();h.tick();assert.equal(h.calls.at(-1),'berry');
  assert.equal(h.events.find(e=>e.event==='berry_requested').before,2);h.command({type:'stop'});
});
test('capture shares the fresh frame inventory instead of repeating six native count calls',async()=>{
  const h=await berryHarness({discard:{}});h.complete({Status:0,ActiveItem:701});
  const before=h.itemCountReads;h.tick();assert.equal(h.calls.at(-1),'capture');assert.equal(h.itemCountReads-before,6);
  h.command({type:'stop'});
});
test('invalid berry configuration cannot reach a game request',async()=>{
  for(const berryOrder of [[],['MasterBall'],['RazzBerry','RazzBerry']])
    await assert.rejects(harness({...berryParams,berryOrder}),/Select berries|Invalid berry/);
});

test('a spin-only session does not require selected berries or send a berry request',async()=>{
  const h=await harness({...berryParams,autoCatch:false,autoSpin:true,berryOrder:[]});h.tick();
  assert.deepEqual(h.calls,['spin']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('automatic berries continue with validated notification-shade focus on the same connection',async()=>{
  const h=await automaticHarness({...shadeParams,...berryParams});
  const proof=h.events.find(e=>e.event==='preflight_validated');assert.ok(proof);
  h.command({type:'activate',...proof});h.tick();h.shadeTick(500);
  h.complete();h.shadeTick(500);assert.deepEqual(h.calls,['encounter','berry']);
  h.complete({Status:0,ActiveItem:701});h.shadeTick(500);assert.equal(h.calls.at(-1),'capture');
  h.complete({Status:1});h.shadeTick(500);h.command({type:'stop'});
  const stopped=h.events.find(e=>e.event==='stopped');assert.equal(stopped.caught,1);assert.equal(stopped.berriesUsed,1);
  assert.equal(h.remainingHandles,0);
});

test('the actual GUI adapter acts after original opening without altering the pending capture',async()=>{
  const h=await harness({...berryParams,skipInterruptions:true});h.tick();h.complete();h.tick();
  assert.deepEqual(h.calls,['encounter','berry']);
  h.openGui(uiProfile.TARGETS[0].name);assert.equal(h.uiActions.length,1);
  assert.equal(h.uiActions[0].originalOpens,1);assert.equal(h.responses.length,1);
  assert.deepEqual(h.calls,['encounter','berry']);
  h.complete({Status:0,ActiveItem:701});h.tick();h.complete({Status:1});h.tick();h.command({type:'stop'});
  assert.equal(h.events.find(event=>event.event==='stopped').caught,1);assert.equal(h.remainingHandles,0);
  assert.ok(h.uiDetached);assert.ok(h.events.find(event=>event.event==='ui_interruption_cleanup').passed);
});

test('a native map callback inside our GUI action cannot reenter the capture engine',async()=>{
  let h;h=await harness({...berryParams,skipInterruptions:true,onUiAction:()=>h.tick(500)});
  h.tick();h.complete();h.tick();h.complete({Status:0,ActiveItem:701});
  h.openGui(uiProfile.TARGETS[0].name);assert.deepEqual(h.calls,['encounter','berry']);
  h.tick();assert.deepEqual(h.calls,['encounter','berry','capture']);h.command({type:'stop'});
  assert.equal(h.remainingHandles,0);
});

test('queued GUI actions can resume while the map reports hidden but cannot run in another app',async()=>{
  const h=await harness({skipInterruptions:true});h.tick();h.tick(300,false);
  h.openGui(uiProfile.TARGETS[1].name);assert.equal(h.uiActions.length,0);
  h.tick(500,false,false,false,false);assert.equal(h.uiActions.length,0);
  h.tick(500,true,true,false,false);assert.equal(h.uiActions.length,1);
  assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('read-only startup and disabled UI settings cannot dismiss interruptions',async()=>{
  for(const options of [{skipInterruptions:true,readOnly:true},{skipInterruptions:false}]) {
    const h=await harness(options);h.tick();h.openGui(uiProfile.TARGETS[0].name);
    assert.equal(h.uiActions.length,0);h.command({type:'stop'});assert.equal(h.remainingHandles,0);
  }
});

test('unsupported GUI metadata disables only the optional UI layer and leaves captures functional',async()=>{
  const h=await harness({skipInterruptions:true,missingGuiProfile:true});h.tick();h.complete();h.tick();
  assert.deepEqual(h.calls,['encounter','capture']);
  assert.ok(h.events.some(event=>event.event==='ui_interruption_profile'&&!event.available));
  assert.ok(!h.events.some(event=>event.event==='error'));h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('Stop with an unfocused queued GUI releases both the UI hook and its full-width handle',async()=>{
  const h=await harness({skipInterruptions:true});h.tick();h.tick(300,false);
  h.openGui(uiProfile.TARGETS[0].name);assert.equal(h.uiActions.length,0);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);assert.ok(h.uiDetached);
  const cleanup=h.events.find(event=>event.event==='ui_interruption_cleanup');assert.ok(cleanup.passed);
  assert.equal(cleanup.remainingReferences,0);h.tick();assert.equal(h.uiActions.length,0);
});

test('an account change on an interruption cannot authorize a GUI action or preserve old references',async()=>{
  const h=await harness({skipInterruptions:true});h.tick();h.setPlayerId('mock_player_B');
  h.openGui(uiProfile.TARGETS[0].name);assert.equal(h.uiActions.length,0);
  assert.ok(h.events.some(event=>event.event==='session_changed'));
  assert.ok(h.events.some(event=>event.event==='stopped'));assert.equal(h.remainingHandles,0);assert.ok(h.uiDetached);
});

test('validated notification-shade authorization applies to GUI events without restarting the connection',async()=>{
  const h=await automaticHarness({...shadeParams,skipInterruptions:true});
  const proof=h.events.find(event=>event.event==='preflight_validated');assert.ok(proof);
  h.command({type:'activate',...proof});h.tick();h.shadeTick(500);
  h.openGui(uiProfile.TARGETS[0].name);assert.equal(h.uiActions.length,1);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);assert.ok(h.uiDetached);
});

test('spin telemetry distinguishes a stop outside range without sending additional game requests',async()=>{
  const h=await harness({autoSpin:true,autoCatch:false,stopLocation:{Latitude:0.001}});h.tick();
  const scan=h.events.find(event=>event.event==='spin_scan').scan;
  assert.equal(scan.reason,'out_of_range');assert.equal(scan.outOfRange,1);
  assert.ok(scan.closestCheckedMeters>100);assert.equal(scan.complete,true);
  assert.deepEqual(h.rpcRequests,[]);h.command({type:'stop'});
});

test('spin telemetry records real cooldowns without changing the five-minute scheduling rule',async()=>{
  const h=await harness({autoSpin:true,autoCatch:false});h.tick();h.complete({Result:1});h.tick();h.tick(3000);
  const scan=h.events.filter(event=>event.event==='spin_scan').at(-1).scan;
  assert.equal(scan.reason,'cooldown');assert.equal(scan.cooldown,1);assert.ok(scan.earliestCooldownMs>290000);
  assert.equal(h.rpcRequests.length,1);h.tick(301000);assert.equal(h.rpcRequests.length,2);
  h.command({type:'stop'});assert.equal(h.remainingHandles,0);
});

test('spin bag-full telemetry has no stop enumeration or unwanted spin request',async()=>{
  const h=await harness({autoSpin:true,bagFull:true});h.tick();
  assert.equal(h.events.find(event=>event.event==='spin_scan').scan.reason,'item_bag_full');
  assert.deepEqual(h.calls,['encounter']);h.command({type:'stop'});
});

test('berry use preserves session-only transfer authorization and inventory reconciliation',async()=>{
  const h=await berryHarness({transferEnabled:true,collectionAccessThrows:true,
    ownedFields:{individualAttack_:4,individualDefense_:8,individualStamina_:9}});
  h.complete({Status:0,ActiveItem:701});h.tick();
  h.complete({Status:1,CapturedPokemonId:{toString:()=>h.ownedId}});h.tick();h.tick(600);
  assert.equal(h.calls.filter(call=>call==='transfer').length,1);assert.equal(h.collectionReads,0);
  assert.ok(h.events.some(event=>event.event==='transfer_decision'&&event.transferAllowed&&event.origin==='session'));
  h.complete({Status:1});h.tick();h.removeOwned();h.tick(600);h.command({type:'stop'});
  const stopped=h.events.find(e=>e.event==='stopped');assert.equal(stopped.transferred,1);assert.equal(stopped.berriesUsed,1);
  assert.equal(h.remainingHandles,0);
});

test('capture quota prevents further encounters, throws and berries after a confirmed catch',async()=>{
  const h=await harness({maxCatches:1,useBerries:true,inventory:{RazzBerry:10}});
  h.tick();h.complete();h.tick();assert.equal(h.complete({Status:0,ActiveItem:701}),'berry');h.tick();
  assert.equal(h.complete({Status:1}),'capture');h.tick();
  h.setEncounterId('18446744073709551613');for(let i=0;i<20;i++)h.tick(301000);
  assert.deepEqual(h.calls,['encounter','berry','capture']);
  assert.equal(h.events.filter(e=>e.event==='session_limit'&&e.kind==='capture').length,1);
  const stopped=h.events.find(e=>e.event==='stopped');assert.equal(stopped.caught,1);
  assert.equal(stopped.limitStopKind,'capture');assert.ok(stopped.cleanupPassed);
  assert.ok(!h.events.some(e=>e.event==='error'));assert.equal(h.remainingHandles,0);
});

test('spin quota stops the whole engine before any encounter or throw can be sent',async()=>{
  const h=await harness({maxSpins:1,maxCatches:1,autoSpin:true,skipInterruptions:true});
  h.tick();assert.equal(h.complete({Result:1}),'spin');h.tick();
  h.setEncounterId('18446744073709551613');for(let i=0;i<10;i++)h.tick(301000);
  assert.deepEqual(h.calls,['spin']);
  assert.equal(h.events.filter(e=>e.event==='session_limit').length,1);
  const stopped=h.events.find(e=>e.event==='stopped');assert.equal(stopped.limitStopKind,'spin');
  assert.equal(stopped.spun,1);assert.ok(stopped.cleanupPassed);
  h.openGui(uiProfile.TARGETS[0].name);assert.equal(h.uiActions.length,0);assert.ok(h.uiDetached);
  assert.ok(!h.events.some(e=>e.event==='error'));
  assert.equal(h.remainingHandles,0);
});

test('capture quota does not dispatch a queued transfer after the terminal response',async()=>{
  const h=await caughtForTransfer({maxCatches:1,maxSpins:2});
  h.setEncounterId('18446744073709551613');h.tick(301000);
  assert.equal(h.calls.filter(c=>c==='capture').length,1);
  assert.equal(h.calls.filter(c=>c==='transfer').length,0);
  assert.ok(h.events.some(e=>e.event==='owned_read_pending_at_stop'&&!e.transferAllowed));
  assert.ok(!h.events.some(e=>e.event==='error'));assert.equal(h.remainingHandles,0);
  assert.equal(h.events.find(e=>e.event==='stopped').transferred,0);
});

test('failed throws do not consume the catch quota and existing retry policy is preserved',async()=>{
  const h=await harness({maxCatches:1,maxThrows:0});h.tick();h.complete();h.tick();
  h.complete({Status:2});h.tick();assert.equal(h.calls.filter(c=>c==='capture').length,2);
  assert.ok(!h.events.some(e=>e.event==='session_limit'));h.complete({Status:1});h.tick();h.tick(301000);
  assert.equal(h.calls.filter(c=>c==='capture').length,2);assert.equal(h.events.find(e=>e.event==='session_limit').caught,1);
  h.command({type:'stop'});
});

test('failed spins do not consume the quota and success disables only later spins',async()=>{
  const h=await harness({maxSpins:1,autoSpin:true,autoCatch:false});h.tick();h.complete({Result:2});h.tick();h.tick(31000);
  assert.equal(h.calls.length,2);assert.ok(!h.events.some(e=>e.event==='session_limit'));
  h.complete({Result:1});h.tick();h.tick(301000);assert.equal(h.calls.length,2);
  assert.equal(h.events.find(e=>e.event==='session_limit').spun,1);h.command({type:'stop'});
});

test('session quotas do not survive a new engine session or change read-only startup',async()=>{
  for(let i=0;i<2;i++) {
    const h=await harness({maxCatches:1});h.tick();h.complete();h.tick();h.complete({Status:1});h.tick();
    assert.equal(h.events.find(e=>e.event==='session_limit').caught,1);h.command({type:'stop'});
  }
  const h=await harness({maxCatches:1,maxSpins:1,readOnly:true,autoSpin:true});h.tick();h.tick(301000);
  assert.equal(h.calls.length,0);assert.ok(!h.events.some(e=>e.event==='session_limit'));h.command({type:'stop'});
});
