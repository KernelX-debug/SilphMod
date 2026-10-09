// Execute our ACTUAL poll/stop/accounting code with synthetic game objects.
// Test-only exposure is added in memory; it is never bundled into the APK.
const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),esbuild=require('esbuild');
const compiled=esbuild.build({entryPoints:['src/agent.ts'],bundle:true,write:false,format:'iife',platform:'neutral',
  plugins:[{name:'test-game-boundary',setup(build){
    build.onResolve({filter:/^frida-(il2cpp|java)-bridge$/},a=>({path:a.path,namespace:'mock'}));
    build.onLoad({filter:/.*/,namespace:'mock'},a=>({contents:a.path==='frida-java-bridge'?
      'export default {};':'globalThis.Il2Cpp=globalThis.__fakeIl2Cpp;',loader:'js'}));
    build.onLoad({filter:/[\\/]agent\.ts$/},a=>({loader:'ts',contents:fs.readFileSync(a.path,'utf8')+`
      globalThis.__agentTest={
        seed(response,options={}){
          config={sessionAccounting:true,actionDelayMs:250,diagnosticTrace:false,inspectOwned:false,transferEnabled:false,...options};
          gc=globalThis.__fakeGc; locationAdapter=globalThis.__fakeLocation; rewardsAvailable=true;
          pending={kind:options.kind??'discard',promise:globalThis.__fakePromise(response),handles:[],started:Date.now(),
            id:'RazzBerry',item:701,before:1,count:1,keep:0,sequence:1,...options.pending};
        },run(){try{poll();}catch(error){fail(error);}},
        canDiscard(){return discard({},false);},userStop(){stop('user');},
        snapshot(){return {stopped,cleanupPaused,pending:!!pending,cleanupPauseReason,...statistics()};}
      };` }));
  }}]}).then(r=>r.outputFiles[0].text);
async function harness() {
  let locationClosed=0,gcClosed=0;const events=[];
  class ObjectValue {
    constructor(value,fields={},methods={}) {
      if(value instanceof ObjectValue)return value;
      this.class={fullName:value,name:String(value).split('.').pop()};this.fields=fields;this.methods=methods;this.handle=this;
    }
    isNull(){return this.class.fullName==='null';}
    tryField(name){if(!Object.hasOwn(this.fields,name))return undefined;
      const value=this.fields[name];return {offset:16,isStatic:false,get value(){if(value instanceof Error)throw value;return value;}};}
    field(name){const value=this.tryField(name);if(!value)throw Error('Missing field '+name);return value;}
    tryMethod(name){return this.methods[name];}
    toString(){return 'fixture';}
  }
  class ValueType {}
  const nullObject=new ObjectValue('null');
  const method=(type,invoke,parameters=[])=>({returnType:{name:type,class:{isEnum:type==='enum'}},
    virtualAddress:{isNull:()=>false},parameters,invoke});
  const list=(values,type='System.Int32')=>new ObjectValue('RepeatedField',{},
    {get_Count:method('System.Int32',()=>values.length),get_Item:method(type,i=>values[i],[{type:{name:'System.Int32'}}])});
  const score=(exp=[100,1000],dust=[150])=>new ObjectValue('Holoholo.Rpc.CaptureScoreProto',
    {Exp:list(exp),Stardust:list(dust),ExperienceFromActiveMega:0,TempEvoScoreInfo:nullObject});
  const item=(type,count)=>new ObjectValue('Holoholo.Rpc.LootItemProto',{Count:count},{get_TypeCase:method('enum',()=>type)});
  const loot=items=>new ObjectValue('Holoholo.Rpc.LootProto',{LootItem:list(items,'Holoholo.Rpc.LootItemProto')});
  const context={rpc:{exports:{}},__fakeIl2Cpp:{Object:ObjectValue,ValueType},
    setInterval:()=>1,clearInterval:()=>{},setImmediate:()=>{},
    Process:{getCurrentThreadId:()=>1},send:message=>events.push(JSON.parse(message[1]).payload),
    __fakePromise:response=>({token:{toString:()=>1},target:new ObjectValue('Promise',
      {errorCalled:false,completeCalled:true,completedValue:response})}),
    __fakeLocation:{close:()=>{locationClosed++;return {passed:true,remainingHooks:0,remainingClassRefs:0,errors:[]};}},
    __fakeGc:{count:1,retain:handle=>({token:{toString:()=>1},target:handle,free(){}}),
      close:()=>{gcClosed++;return {passed:true,remaining:0,errors:[]};}},
  };
  context.__fakeIl2Cpp.Object.headerSize=16;
  vm.runInNewContext(await compiled,context);
  return {api:context.__agentTest,events,ObjectValue,nullObject,list,score,item,loot,
    get locationClosed(){return locationClosed;},get gcClosed(){return gcClosed;}};
}
test('actual poll pauses an ambiguous discard, keeps location treatment and never repeats recycling',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.RecycleItemOutProto',{Result:1,NewCount:1}));h.api.run();
  assert.equal(h.api.snapshot().stopped,false);assert.equal(h.api.snapshot().cleanupPaused,true);
  assert.equal(h.api.snapshot().pending,false);assert.equal(h.api.snapshot().discarded,0);
  assert.equal(h.api.canDiscard(),false);assert.equal(h.locationClosed,0);assert.equal(h.gcClosed,0);
  assert.equal(h.events.find(e=>e.event==='discard_response').newCount,1);
  assert.equal(h.events.find(e=>e.event==='cleanup_paused').retryAllowed,false);
  assert.ok(!h.events.some(e=>e.event==='error'||e.event==='stopped'));
  h.api.userStop();assert.equal(h.locationClosed,1);assert.equal(h.gcClosed,1);
});
test('actual poll treats a valid not-enough-copies rejection as a cleanup-only pause',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.RecycleItemOutProto',{Result:2,NewCount:0}));h.api.run();
  assert.equal(h.api.snapshot().cleanupPauseReason,'not_enough_copies');assert.equal(h.api.snapshot().stopped,false);
});
test('actual poll confirms a valid discard and starts reconciliation instead of pausing',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.RecycleItemOutProto',{Result:1,NewCount:0}));h.api.run();
  assert.equal(h.api.snapshot().discarded,1);assert.equal(h.api.snapshot().cleanupPaused,false);
  assert.equal(h.events.find(e=>e.event==='discard').confirmed,true);
});
test('actual native fault/wrong response class/floor breach still stop and release location',async()=>{
  for(const [type,fields,keep] of [['Wrong.Class',{Result:1,NewCount:0},0],
    ['Holoholo.Rpc.RecycleItemOutProto',{Result:1,NewCount:0},1],
    ['Holoholo.Rpc.RecycleItemOutProto',{Result:new Error('access violation accessing 0x1234'),NewCount:0},0]]) {
    const h=await harness();h.api.seed(new h.ObjectValue(type,fields),{pending:{before:2,count:1,keep}});h.api.run();
    assert.equal(h.api.snapshot().stopped,true);assert.equal(h.locationClosed,1);
    assert.ok(h.events.some(e=>e.event==='error'));
  }
});
test('legacy callers are not silently switched to the new cleanup recovery policy',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.RecycleItemOutProto',{Result:2,NewCount:0}),{sessionAccounting:false});h.api.run();
  assert.equal(h.api.snapshot().stopped,true);
});
test('actual capture response produces XP/dust from retained score and no extra requests',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.CatchPokemonOutProto',{Status:1,Scores:h.score()}),{kind:'capture'});h.api.run();
  assert.equal(h.api.snapshot().stopped,false);assert.equal(h.api.snapshot().xp,1100);assert.equal(h.api.snapshot().stardust,150);
  assert.deepEqual(h.events.find(e=>e.event==='reward_receipt').reward,{xp:1100,stardust:150});
});

test('the terminal catch credits its real reward before closing both adapters and acknowledging the limit',async()=>{
  const h=await harness();
  h.api.seed(new h.ObjectValue('Holoholo.Rpc.CatchPokemonOutProto',{Status:1,Scores:h.score()}),
    {kind:'capture',autoCatch:true,maxCatches:1,maxSpins:2});h.api.run();
  const stopped=h.events.find(e=>e.event==='stopped');
  assert.equal(stopped.limitStopKind,'capture');assert.equal(stopped.caught,1);
  assert.equal(stopped.xp,1100);assert.equal(stopped.stardust,150);assert.ok(stopped.cleanupPassed);
  assert.ok(h.events.findIndex(e=>e.event==='reward_receipt')<h.events.findIndex(e=>e.event==='session_limit'));
  assert.equal(h.locationClosed,1);assert.equal(h.gcClosed,1);
});

test('selected throw quality never adds XP that the confirmed capture score did not award',async()=>{
  for(const throwQuality of ['NORMAL','NICE','GREAT','EXCELLENT']) {
    const h=await harness();
    h.api.seed(new h.ObjectValue('Holoholo.Rpc.CatchPokemonOutProto',{Status:1,Scores:h.score([100,20],[100])}),
      {kind:'capture',throwQuality});h.api.run();
    assert.equal(h.api.snapshot().xp,120);assert.equal(h.api.snapshot().stardust,100);
    assert.equal(h.api.snapshot().stopped,false);
  }
});

test('a failed throw does not credit XP even if a score field is present',async()=>{
  const h=await harness();
  h.api.seed(new h.ObjectValue('Holoholo.Rpc.CatchPokemonOutProto',{Status:3,Scores:h.score()}),
    {kind:'capture',throwQuality:'EXCELLENT'});h.api.run();
  assert.equal(h.api.snapshot().xp,0);assert.equal(h.api.snapshot().stardust,0);
  assert.equal(h.api.snapshot().caught,0);assert.equal(h.api.snapshot().fled,1);
});
test('actual spin includes XP awarded and reward loot, not physical items',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.FortSearchOutProto',
    {Result:1,XpAwarded:100,Loot:h.loot([h.item(6,500),h.item(1,4)]),BonusLoot:h.loot([h.item(2,100)])}),{kind:'spin'});h.api.run();
  assert.equal(h.api.snapshot().xp,600);assert.equal(h.api.snapshot().stardust,100);assert.equal(h.api.snapshot().spun,1);
});
test('missing optional score marks partial telemetry without stopping or inventing XP',async()=>{
  const h=await harness();h.api.seed(new h.ObjectValue('Holoholo.Rpc.CatchPokemonOutProto',{Status:1,Scores:h.nullObject}),{kind:'capture'});h.api.run();
  assert.equal(h.api.snapshot().stopped,false);assert.equal(h.api.snapshot().xp,0);assert.equal(h.api.snapshot().missingRewardReceipts,1);
  assert.ok(h.events.some(e=>e.event==='reward_unavailable'));
});
