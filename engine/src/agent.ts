import 'frida-il2cpp-bridge';
import Java from 'frida-java-bridge';
import {createGcApi64,GcReferences64,HeldReference64} from './gc-handles.js';
// @ts-ignore Own CommonJS policy shared with the mock tests.
import policy from './policy.cjs';
// @ts-ignore Pure, independently tested read-only policy.
import inspection from './pokemon-inspection.cjs';
// @ts-ignore Own scope ledger and fail-closed transfer policy.
import transferPolicy from './session-transfers.cjs';
// @ts-ignore Factual catalog plus our explicit discard allowlist.
import itemCatalog from './item-catalog.cjs';
// @ts-ignore Own account-bound lifecycle policy.
import runtimePolicy from './runtime-policy.cjs';
// @ts-ignore Own single-connection startup policy.
import startupPolicy from './startup-gate.cjs';
// @ts-ignore Independent JNI ABI adapter. Disabled unless explicitly requested by the UGPhone host.
import nativeLocation from './native-location.cjs';
// @ts-ignore Independent receipt accounting and conservative recycling recovery.
import rewardPolicy from './session-rewards.cjs';
// @ts-ignore Validated response values only; native failures are not downgraded.
import discardResponse from './discard-response.cjs';
// @ts-ignore Controller timing only: independent of native objects and RPC outcomes.
import controlWatchdog from './control-watchdog.cjs';
// @ts-ignore
import mapObservation from './map-observation.cjs';
// @ts-ignore
import throwProfile from './throw-profile.cjs';
// @ts-ignore
import berryPolicy from './berry-policy.cjs';
// @ts-ignore
import uiInterruptions from './ui-interruptions.cjs';
// @ts-ignore
import spinObservation from './spin-observation.cjs';
// @ts-ignore
import sessionLimits from './session-limits.cjs';
import {RewardDataError,captureRewardData,spinRewardData} from './response-rewards.js';

type Obj = Il2Cpp.Object;
type Location = {latitude:number; longitude:number};
type Config = {autoCatch:boolean; autoSpin:boolean; cleanBag:boolean; readOnly:boolean;
  maxThrows:number; actionDelayMs:number; maxCatches:number;maxSpins:number;discard:Record<string,number>; gamePackage:string;
  diagnosticTrace:boolean;encounterOnly:boolean;inspectOwned:boolean;transferEnabled:boolean;
  keepShiny:boolean;keepHundo:boolean;maxTransfers:number;sessionId:string;maxDiscardedItems:number;expectedPlayerId:string;
  automaticPreflight:boolean;sessionAccounting:boolean;controlRecovery:boolean;
  throwQuality:string;notificationShadeSupport:boolean;useBerries:boolean;berryOrder:string[];skipInterruptions:boolean};
type CaptureRequest = {sessionId:string;requestId:string;encounterId:string;sentAt:number};
type Pending = {kind:'encounter'|'berry'|'capture'|'spin'|'discard'|'transfer'; promise:HeldReference64;
  handles:HeldReference64[]; started:number; id:string; item?:number; count?:number; before?:number;keep?:number;
  captureRequest?:CaptureRequest;freshPromise?:boolean;sequence?:number};
let config:Config, initialized=false, stopped=false, ready=false, lastBeat=0, allowed=false;
let pending:Pending|null=null, target:HeldReference64|null=null, throws=0;
let berryProfileReady=false, berryCycle:any=null;
let nextAction=0, nextSpin=0, nextInventory=0, nextReport=0, lastTick=0;
let hook:InvocationListener|null=null, inTick=false, container:Obj|null=null;
let uiAdapter:any=null,unityThreadId=0;
let containerHandle:HeldReference64|null=null, gc:GcReferences64|null=null, activity:any=null, javaReady=false;
let lastWindowCheck=0, windowFocused=false;
let notificationShade=false;
let inventorySync:{name:string;remaining:number;started:number;keep:number}|null=null;
let nextBlockedReport=0;
let startupCleanupPending=false, nextInventoryReport=0, lastInventorySummary='';
const ownedReads=new Map<string,{encounterId:string;started:number;nextRead:number;inspected:boolean}>();
let transferLedger:any=null, preflightStarted=false, transferLimitReported=false;
let discardLimitReported=false;
let cleanupPaused=false,cleanupPauseReason='',requestSequence=0;
const rewards=new rewardPolicy.SessionRewards();
let rewardsAvailable=false;
let controlDelayed=false,lastHeartbeatSequence=0,lastHeartbeatSentAt=0;
let transferSync:{id:string;encounterId:string;started:number;nextRead:number}|null=null;
let transferDispatch:{id:string;encounterId:string}|null=null;
let stage='bootstrap';
let traceSequence=0,encounterAttempted=false;
let readSamples=0,readSampleAt=0,readSampleStarted=0;
let sessionPlayerId='', sessionPlayerAddress='', sessionDirectoryAddress='';
let lastSessionCheck=0, guardMaxMs=0, nextHealth=0, sessionReadyAt=0, lastRequestAt=0;
let nextCacheSweep=0, nextCaptureNotice=0, operationalState='starting';
let capturePauseReason='', targetSelectedAt=0, skippedTargets=0, nextScanReport=0;
let lastMapScan:Record<string,number>={};
const mapObserver=new mapObservation.MapObserver();
const spinObserver=new spinObservation.SpinObserver();
let nextSpinBlockedReport=0,spinBagBlocked=false;
let storageCache:{at:number;pokemonCount:number;maxPokemon:number;pokemonBagFull:boolean}|null=null;
let startupGate:any=null,activationRequest:any=null;
let locationAdapter:any=null;
let locationRequested=false;
let eventObserver:((event:string)=>void)|null=null;
const CAPTURE_RANGE_METERS=40, LOST_TARGET_COOLDOWN_MS=5000;
let nextTraceSummary=0;
const repeatedStages=new Map<string,number>(), stageLastEmitted=new Map<string,number>();
const seen=new Map<string,number>(), stops=new Map<string,number>();
const stats={caught:0,fled:0,spun:0,discarded:0,transferred:0,inspected:0,retained:0,review:0,unknown:0,berriesUsed:0};
let limitStopKind='';
const services=new Map<string,HeldReference64>();
function statistics() {
  const counts={...stats,...sessionLimits.state(config??{},stats)};
  return config?.sessionAccounting?{...counts,...rewards.snapshot(),rewardsAvailable,cleanupPaused,cleanupPauseReason}:counts;
}
function captureAllowed():boolean {return stats.caught<(config.maxCatches??sessionLimits.DEFAULT_CATCHES);}
function spinAllowed():boolean {return stats.spun<(config.maxSpins??sessionLimits.DEFAULT_SPINS);}
function reportSessionLimits():boolean {
  const limits=sessionLimits.state(config,stats);
  for(const [kind,reached] of [['capture',limits.catchLimitReached],['spin',limits.spinLimitReached]] as const) {
    if(reached) {
      limitStopKind=kind;
      emit('session_limit',{kind,...limits,caught:stats.caught,spun:stats.spun,scope:'SESSION',stopSession:true,closeGameRequested:true});
      stop(kind==='capture'?'Session capture limit reached':'Session spin limit reached');
      return true;
    }
  }
  return false;
}
function rewardPreflight() {
  if(!config.sessionAccounting)return;
  for(const [type,names] of [
    ['CatchPokemonOutProto',['Scores']],['CaptureScoreProto',['Exp','Stardust','ExperienceFromActiveMega','TempEvoScoreInfo']],
    ['FortSearchOutProto',['XpAwarded','Loot','BonusLoot']],['LootProto',['LootItem']],['LootItemProto',['Count']],
  ] as [string,string[]][]) {
    const schema=klass('holo-protos','Holoholo.Rpc.'+type);
    for(const name of names) {
      const f=schema.tryField(name);
      if(!f || f.isStatic || f.offset<Il2Cpp.Object.headerSize) {
        emit('reward_profile',{available:false,reason:'field_unavailable',type,field:name});return;
      }
    }
  }
  rewardsAvailable=true;
  emit('reward_profile',{available:true,scope:'CONFIRMED_SESSION_CAPTURE_AND_SPIN_RESPONSES',
    balanceDifference:false,localMultipliers:false,accountRequestsSent:0});
}
function accountReward(p:Pending,result:Obj,status:number) {
  if(!config.sessionAccounting || status!==1 || !['capture','spin'].includes(p.kind))return;
  let reward:any=null,data:any=null,reason='reward_profile_unavailable';
  if(rewardsAvailable) {
    try {
      // Native failures propagate. Only our explicit data/schema validation
      // errors are telemetry-only; no arbitrary game/ABI exception is hidden.
      if(p.kind==='capture')data=captureRewardData(result);else data=spinRewardData(result);
      try {reward=p.kind==='capture'?rewardPolicy.capture(data):rewardPolicy.spin(data.xpAwarded,data.loot,data.bonusLoot);}
      catch(error){throw new RewardDataError(String(error));}
    }catch(error){if(!(error instanceof RewardDataError))throw error;reason=String(error);}
  }
  const settled=rewards.settle(p.sequence,reward);
  emit(reward===null?'reward_unavailable':'reward_receipt',{sequence:p.sequence,kind:p.kind,id:p.id,status,
    source:'retained_game_response',reason:reward===null?reason:settled.reason,added:settled.added,
    reward,data,...rewards.snapshot(),rewardsAvailable});
}
function emit(event:string, detail:Record<string,unknown>={}) {
  eventObserver?.(event);
  // Ordinary send() is printed through buffered stdio by frida-inject. Its
  // dedicated stdout route flushes every message even when stdout is a pipe.
  send(['frida:stdout',JSON.stringify({type:'send',payload:{protocol:1,event,time:Date.now(),...detail}})+'\n']);
}
function fail(error:unknown) {
  const stack=error instanceof Error?error.stack??'':'';
  emit('error',{message:String(error),stage,stack:stack.slice(0,8192),stackTruncated:stack.length>8192});
  stop('Engine error ['+stage+']: '+String(error));
}
function enterStage(name:string,detail:Record<string,unknown>={}) {
  const changed=stage!==name;
  stage=name;
  // Startup diagnostics are emitted only while validating; active actions
  // still set the error context without flooding the bounded host log.
  if(!ready)emit('stage',{stage,...detail});
  else if(changed)trace('stage',{stage,...detail});
}
function trace(phase:string,detail:Record<string,unknown>={}) {
  if(!config?.diagnosticTrace)return;
  // Preserve every action/promise trace and raw value. Only repetitive ACTIVE
  // stage transitions are counted and sampled; read-only remains fully traced.
  if(ready&&!config.readOnly&&phase==='stage') {
    const now=Date.now(),name=String(detail.stage??stage);
    repeatedStages.set(name,(repeatedStages.get(name)??0)+1);
    if(now-(stageLastEmitted.get(name)??0)<30000)return;
    stageLastEmitted.set(name,now);
  }
  emit('trace',{sequence:++traceSequence,phase,stage,tid:Process.getCurrentThreadId(),...detail});
}
function inspectHeld(reference:HeldReference64,source:string):Obj {
  trace('gc_target:before',{source,token:reference.token.toString()});
  const value=heldObject(reference,source);
  trace('gc_target:after',{source,address:value.handle.toString(),class:value.class.fullName});return value;
}
function observed<T>(name:string,call:()=>T):T {
  const began=Date.now();trace(name+':before');
  const value=call();trace(name+':after',{durationMs:Date.now()-began,valueType:valueTypeName(value)});return value;
}
function valueTypeName(value:any):string {
  if(value===null)return 'null';
  if(value===undefined)return 'undefined';
  if(value instanceof Il2Cpp.ValueType)return 'Il2Cpp.ValueType('+value.type.name+')';
  if(value instanceof Il2Cpp.Object)return value.isNull()?'Il2Cpp.Object(null)':value.class.fullName;
  return typeof value;
}
function klass(assembly:string,name:string):Il2Cpp.Class {
  return Il2Cpp.domain.assembly(assembly).image.class(name);
}
function object(value:any,source='game object'):Obj {
  if (!(value instanceof Il2Cpp.Object) || value.isNull())
    throw Error(source+': expected a non-null Il2Cpp.Object; got '+valueTypeName(value));
  return value;
}
function number(value:any):number {
  return typeof value==='number'?value:value instanceof Il2Cpp.ValueType?
    Number(value.field('value__').value):Number(value.toString());
}
function proto(name:string):Obj {
  const obj=klass('holo-protos','Holoholo.Rpc.'+name).alloc(); obj.method('.ctor',0).invoke(); return obj;
}
function field(obj:Obj,name:string,value:any) { obj.field(name).value=value; }
function retain(obj:Obj,label:string):HeldReference64 {
  if(!gc)throw Error('GCHandle64 manager not initialized');
  return gc.retain(object(obj,label).handle,label);
}
function heldObject(reference:HeldReference64,source:string):Obj {
  return object(new Il2Cpp.Object(reference.target),source);
}
function findService(name:string,assembly='holo-game'):Obj {
  enterStage('service:'+name);
  const cached=services.get(name); if(cached) return heldObject(cached,'Retained service '+name);
  const type=klass(assembly,name);
  if (!container) {
    const contexts=object(klass('Zenject','Zenject.Internal.ZenUtilInternal').method<Obj>('GetAllSceneContexts',0).invoke(),'ZenUtilInternal.GetAllSceneContexts');
    // Loading/tutorial scenes may coexist with the map scene. Do not assume
    // that the first SceneContext is the current game's service container.
    for(const context of enumerate(contexts,64)) {
      const candidate=context.field<Obj>('_container').value;
      if(candidate.isNull())continue;
      const value=candidate.method<Obj>('TryResolve',1).overload('System.Type').invoke(type.type.object);
      if(!value.isNull()){containerHandle=retain(candidate,'Zenject.Container');container=candidate;break;}
    }
    if(!container)throw Error('Service not found in active scenes: '+name);
  }
  if(!containerHandle)throw Error('Container reference missing');
  container=heldObject(containerHandle,'Zenject.Container');
  const value=object(container.method<Obj>('TryResolve',1).overload('System.Type').invoke(type.type.object),'DiContainer.TryResolve('+name+')');
  services.set(name,retain(value,'service:'+name));
  emit('service_resolved',{service:name,concreteClass:value.class.fullName});return value;
}
function sessionChanged(reason:string,observedPlayerId:string|null=null):never {
  emit('session_changed',{reason,previousPlayerId:sessionPlayerId,observedPlayerId,
    repeatReadOnlyRequired:true,automaticRestart:false});
  throw Error('Account/scene changed or could not be verified ('+reason+'); stop and reopen the map');
}
function validateSession(force=false) {
  if(!sessionPlayerId)return;
  const began=Date.now();if(!force&&began-lastSessionCheck<1000)return;
  if(!containerHandle)sessionChanged('container_missing');
  const contexts=object(klass('Zenject','Zenject.Internal.ZenUtilInternal').method<Obj>('GetAllSceneContexts',0).invoke(),'Session.SceneContexts');
  const containers=enumerate(contexts,64).map(context=>context.field<Obj>('_container').value);
  const current=containers.find(value=>!value.isNull()&&value.handle.toString()===containerHandle!.target.toString());
  if(!current)sessionChanged('scene_removed');
  // Resolve afresh: pinned cached services alone cannot identify a new login.
  const player=current.method<Obj>('TryResolve',1).overload('System.Type')
    .invoke(klass('holo-game','Niantic.Holoholo.IPlayerService').type.object);
  const directory=current.method<Obj>('TryResolve',1).overload('System.Type')
    .invoke(klass('holo-game','Niantic.Holoholo.Map.IMapPlaceDirectoryService').type.object);
  if(!(player instanceof Il2Cpp.Object)||player.isNull()||player.handle.toString()!==sessionPlayerAddress)
    sessionChanged('player_service_replaced');
  if(!(directory instanceof Il2Cpp.Object)||directory.isNull()||directory.handle.toString()!==sessionDirectoryAddress)
    sessionChanged('map_service_replaced');
  const value=player.method<Il2Cpp.String>('get_PlayerId',0).invoke().content;
  if(!runtimePolicy.playerIdValid(value)||value!==sessionPlayerId)sessionChanged('player_id_changed',value);
  lastSessionCheck=Date.now();guardMaxMs=Math.max(guardMaxMs,lastSessionCheck-began);
}
function bindSession(directory:Obj) {
  const player=findService('Niantic.Holoholo.IPlayerService');
  const id=player.method<Il2Cpp.String>('get_PlayerId',0);
  if(id.virtualAddress.isNull())throw Error('PlayerId reading unavailable');
  const value=id.invoke().content;
  if(!runtimePolicy.playerIdValid(value))throw Error('PlayerId unavailable; wait for the logged-in map');
  sessionPlayerId=value!;sessionPlayerAddress=player.handle.toString();sessionDirectoryAddress=directory.handle.toString();
  if(config.expectedPlayerId&&config.expectedPlayerId!==sessionPlayerId)sessionChanged('read_only_player_mismatch',sessionPlayerId);
  validateSession(true);
  emit('session_identity',{playerId:sessionPlayerId,playerService:sessionPlayerAddress,mapService:sessionDirectoryAddress,
    container:containerHandle!.target.toString(),passed:true,gameRequestsSent:0});
}
function pokemonStorage() {
  const now=Date.now();if(storageCache&&now-storageCache.at<500)return storageCache;
  enterStage('storage:pokemon');
  const count=findService('Niantic.Holoholo.IPokemonBag').method('get_Count',0);
  const maximum=findService('Niantic.Holoholo.IPlayerService').method('get_MaxPokemon',0);
  for(const method of [count,maximum]) {
    if(method.virtualAddress.isNull()||method.returnType.name!=='System.Int32')
      throw Error('Incompatible Pokémon count/capacity signature');
  }
  const pokemonCount=number(count.invoke()),maxPokemon=number(maximum.invoke());
  if(!Number.isInteger(pokemonCount)||pokemonCount<0||!Number.isInteger(maxPokemon)||maxPokemon<=0)
    throw Error('Invalid Pokémon count/capacity; item indicators are not used');
  // PlayerService.BagIsFull is ITEM capacity (IItemBag), not Pokemon capacity.
  // Count reads a scalar; it does not enumerate IDs or authorize old transfers.
  return storageCache={at:now,pokemonCount,maxPokemon,pokemonBagFull:pokemonCount>=maxPokemon};
}
function reportHealth(now:number,counts:Record<string,number>) {
  if(now<nextHealth)return;nextHealth=now+15000;
  const player=findService('Niantic.Holoholo.IPlayerService');
  emit('health',{playerId:sessionPlayerId,elapsedMs:now-sessionReadyAt,state:operationalState,
    pending:pending?.kind??null,pendingAgeMs:pending?now-pending.started:0,
    lastRequestAgeMs:lastRequestAt?now-lastRequestAt:null,heartbeatAgeMs:now-lastBeat,
    gcReferences:gc?.count??0,ownedQueue:ownedReads.size,transferSync:!!transferSync,inventorySync:!!inventorySync,
    seenCache:seen.size,stopCache:stops.size,guardMaxMs,counts,
    totalItems:number(findService('Niantic.Holoholo.IItemBag').method('GetTotalItems',0).invoke()),
    maxItems:number(player.method('get_MaxItems',0).invoke()),
    itemBagFull:player.method<boolean>('get_BagIsFull',0).invoke(),...pokemonStorage(),
    targetAgeMs:target?now-targetSelectedAt:null,skippedTargets,mapScan:lastMapScan,
    spinScan:spinObserver.snapshot(now),uiInterruptions:uiAdapter?.snapshot()??null,...statistics()});
}
function enumerate(sequence:Obj,limit=256):Obj[] {
  const method=sequence.tryMethod<Obj>('GetEnumerator',0)??sequence.method<Obj>('System.Collections.IEnumerable.GetEnumerator',0);
  // Prefer the non-generic interface if GetEnumerator returns an unboxed struct.
  let enumerator:any=method.invoke();
  if (enumerator instanceof Il2Cpp.ValueType) enumerator=enumerator.box();
  const cursor=object(enumerator,'GetEnumerator'), result:Obj[]=[];
  try {
    while(result.length<limit && cursor.method<boolean>('MoveNext',0).invoke()) {
      let item=cursor.tryMethod<Obj>('get_Current',0)??cursor.method<Obj>('System.Collections.IEnumerator.get_Current',0);
      const value=item.invoke(); if(value instanceof Il2Cpp.Object && !value.isNull()) result.push(value);
    }
  } finally { cursor.tryMethod('Dispose',0)?.invoke(); }
  return result;
}
function getInventory(fullCatalog=false):{bag:Obj;counts:Record<string,number>} {
  const bag=findService('Niantic.Holoholo.IItemBag'), counts:Record<string,number>={};
  enterStage('inventory:counts');
  const item=klass('holo-protos','Holoholo.Rpc.Item');
  const required=new Set(['PokeBall','GreatBall','UltraBall',...Object.keys(config.discard),
    ...(config.useBerries&&config.autoCatch?config.berryOrder:[])]);
  const entries=fullCatalog?itemCatalog.CATALOG:itemCatalog.CATALOG.filter((entry:any)=>required.has(entry.name));
  const snapshot:Record<string,unknown>[]=[];
  for (const entry of entries) {
    try {
      const value=item.field(entry.name).value;
      if(number(value)!==entry.id)throw Error('Incompatible enum ID: '+entry.name);
      const count=number(bag.method('GetItemCount',1).invoke(value));
      if(!Number.isInteger(count)||count<0||count>100000)throw Error('Incompatible item count: '+entry.name);
      counts[entry.name]=count;
      if(fullCatalog)snapshot.push({...entry,available:true,count});
    } catch(error) {
      if(required.has(entry.name))throw error;
      if(fullCatalog)snapshot.push({...entry,available:false,count:null,reason:String(error)});
    }
  }
  if(fullCatalog)emit('inventory_catalog',{items:snapshot,configuredLimits:config.discard,maxDiscardedItems:config.maxDiscardedItems,
    cleanupTrigger:policy.CLEANUP_TRIGGER,unconfiguredTypesPreserved:true,protectedTypesDiscardable:false,gameRequestsSent:0});
  return {bag,counts};
}
function inspectPokemon(value:Obj,origin:'sample'|'session',expectedId?:string,encounterId?:string) {
  const held=retain(value,'inspection:PokemonProto');
  try {
    const pokemon=heldObject(held,'Inventory Pokémon');
    if(pokemon.class.fullName!=='Holoholo.Rpc.PokemonProto')throw Error('Incompatible inventory class: '+pokemon.class.fullName);
    const id=pokemon.field<UInt64>('id_').value.toString();
    if(!inspection.ownedId(id)||expectedId&&id!==expectedId)throw Error('Incompatible inventory identifier');
    const fort=pokemon.field<Il2Cpp.String>('deployedFortId_').value;
    const station=pokemon.field<Il2Cpp.String>('deployedStationId_').value;
    const data={id,dexId:number(pokemon.field('pokemonId_').value),
      attack:number(pokemon.field('individualAttack_').value),defense:number(pokemon.field('individualDefense_').value),
      stamina:number(pokemon.field('individualStamina_').value),shiny:pokemon.method<boolean>('get_IsShinyPokemon',0).invoke(),
      favorite:pokemon.field<boolean>('favorite_').value,egg:pokemon.field<boolean>('isEgg_').value,
      deployed:!!(fort.content||station.content)};
    const verdict=inspection.classifyPokemon(data);
    if(origin==='sample'&&verdict.decision==='unknown')throw Error('Incompatible sample data; active inspection disabled');
    emit('pokemon_inspection',{origin,encounterId:encounterId??null,...data,...verdict});
    if(origin==='session'){
      stats.inspected++;
      if(verdict.decision==='retain')stats.retained++;
      else if(verdict.decision==='review')stats.review++;
      else stats.unknown++;
    }
    return data;
  } finally {held.free();}
}
function inspectOwnedPreflight() {
  enterStage('inspection:preflight');
  const bag=findService('Niantic.Holoholo.IPokemonBag');
  const get=bag.method<Obj>('GetPokemon',1);
  if(get.parameters[0].type.name!=='System.UInt64'||get.virtualAddress.isNull())throw Error('Incompatible GetPokemon signature');
  const type=klass('holo-protos','Holoholo.Rpc.PokemonProto');
  for(const name of ['id_','pokemonId_','individualAttack_','individualDefense_','individualStamina_',
    'favorite_','isEgg_','deployedFortId_','deployedStationId_'])
    if(type.field(name).offset<Il2Cpp.Object.headerSize)throw Error('Incompatible inventory field: '+name);
  if(type.method('get_IsShinyPokemon',0).virtualAddress.isNull())throw Error('Shiny reading unavailable');
  klass('holo-protos','Holoholo.Rpc.CatchPokemonOutProto').field('CapturedPokemonId');
  // Only read-only preflight samples the existing collection. Active farming
  // inspects ONLY IDs returned by its successful capture responses.
  if(!config.readOnly)return;
  enterStage('inspection:sample');
  const sequence=object(bag.method<Obj>('get_AllPokemon',0).invoke(),'PokemonBag.AllPokemon');
  const sequenceHandle=retain(sequence,'inspection:collection');
  let cursorHandle:HeldReference64|null=null;
  let sampled=0;
  try {
    const method=sequence.tryMethod<Obj>('GetEnumerator',0)??sequence.method<Obj>('System.Collections.IEnumerable.GetEnumerator',0);
    let raw:any=method.invoke();if(raw instanceof Il2Cpp.ValueType)raw=raw.box();
    const cursor=object(raw,'PokemonBag.GetEnumerator');cursorHandle=retain(cursor,'inspection:enumerator');
    try {
      while(sampled<8&&cursor.method<boolean>('MoveNext',0).invoke()) {
        const current=cursor.tryMethod<Obj>('get_Current',0)??cursor.method<Obj>('System.Collections.IEnumerator.get_Current',0);
        const sample=object(current.invoke(),'PokemonBag.Current');
        const sampleHandle=retain(sample,'inspection:sample');
        try {
          const id=sample.field<UInt64>('id_').value.toString();
          if(!inspection.ownedId(id))throw Error('Incompatible sample identifier');
          const resolved=object(get.invoke(new UInt64(id)),'PokemonBag.GetPokemon(sample)');
          const resolvedHandle=retain(resolved,'inspection:resolved-sample');
          try {
            const safe=heldObject(resolvedHandle,'Retained sample');
            inspectPokemon(safe,'sample',id);
            if(config.transferEnabled) {
              const data=readTransferData(safe),verdict=transferPolicy.transferVerdict(data,config);
              if(verdict.decision==='unknown')throw Error('Invalid sample protections; transfers disabled');
              emit('transfer_sample',{origin:'sample',...data,...verdict,transferAllowed:false,
                wouldTransfer:verdict.transferAllowed,transferRequestsSent:0});
            }
            sampled++;
          } finally {resolvedHandle.free();}
        } finally {sampleHandle.free();}
      }
    } finally {cursor.tryMethod('Dispose',0)?.invoke();}
  } finally {cursorHandle?.free();sequenceHandle.free();}
  emit('inspection_preflight',{passed:true,sampled,maxSamples:8,transferRequestsSent:0});
  if(config.transferEnabled)emit('transfer_validation',{passed:true,sampled,maxSamples:8,transferRequestsSent:0});
}
function queueOwnedRead(result:Obj,encounterId:string,scopeAccepted:boolean) {
  const id=result.field<UInt64>('CapturedPokemonId').value.toString();
  if(!inspection.ownedId(id)){stats.unknown++;emit('pokemon_inspection',{origin:'session',encounterId,
    id,decision:'unknown',reasons:['invalid_captured_id'],transferAllowed:false});return;}
  if(config.transferEnabled&&!scopeAccepted) {
    emit('transfer_skipped',{id,encounterId,reason:'unproven_current_session_capture'});return;
  }
  if(ownedReads.has(id))return;
  if(ownedReads.size>=64){stats.unknown++;emit('pokemon_inspection',{origin:'session',id,encounterId,
    decision:'unknown',reasons:['inspection_queue_full'],transferAllowed:false});
    if(config.transferEnabled)transferLedger.finish(id,false);return;}
  ownedReads.set(id,{encounterId,started:Date.now(),nextRead:Date.now()+200,inspected:false});
  emit('owned_read_queued',{ownedId:id,encounterId});
}
function pollOwnedReads(now:number):boolean {
  for(const [id,task] of ownedReads) {
    if(now<task.nextRead)continue;
    if(task.inspected) {
      if(transferSync||now<nextAction)return false;
      ownedReads.delete(id);tryTransfer(id,task.encounterId);return pending!==null;
    }
    enterStage('inspection:owned');
    const bag=findService('Niantic.Holoholo.IPokemonBag');
    const pokemon=bag.method<Obj>('GetPokemon',1).invoke(new UInt64(id));
    if(pokemon instanceof Il2Cpp.Object&&!pokemon.isNull()) {
      inspectPokemon(pokemon,'session',id,task.encounterId);task.inspected=true;
      if(config.transferEnabled&&(transferSync||now<nextAction))return false;
      ownedReads.delete(id);
      if(config.transferEnabled)tryTransfer(id,task.encounterId);
      return pending!==null;
    }
    if(now-task.started>=15000){stats.unknown++;ownedReads.delete(id);
      emit('pokemon_inspection',{origin:'session',id,encounterId:task.encounterId,decision:'unknown',
        reasons:['inventory_sync_timeout'],transferAllowed:false});
      if(config.transferEnabled)transferLedger.finish(id,false);return false;}
    task.nextRead=now+200;
    // Round-robin unresolved IDs. Waiting for one update must not freeze farming.
    ownedReads.delete(id);ownedReads.set(id,task);
    return false; // At most one lookup per tick, but no artificial wait for next capture.
  }
  return false;
}
function transferPreflight() {
  enterStage('transfer:preflight');
  const release=findService('Niantic.Holoholo.IPokemonBag').method<Obj>('ReleasePokemon',1);
  const buddy=findService('Niantic.Holoholo.Buddy.IBuddyPokemonService').method<boolean>('GetIsBuddy',1);
  const getClass=klass('holo-protos','Holoholo.Rpc.PokemonProto').method('GetClass',1);
  if(release.virtualAddress.isNull()||release.parameters[0].type.name!=='Holoholo.Rpc.PokemonProto'
    ||buddy.virtualAddress.isNull()||buddy.parameters[0].type.name!=='System.UInt64'
    ||getClass.virtualAddress.isNull()||getClass.parameters[0].type.name!=='IGameMasterData')
    throw Error('Incompatible transfer/protection signature; no actions sent');
  findService('IGameMasterData','holo-protos');
  const type=klass('holo-protos','Holoholo.Rpc.PokemonProto');
  for(const name of ['isFusion_','limitedPokemonIdentifier_'])
    if(type.field(name).offset<Il2Cpp.Object.headerSize)throw Error('Incompatible transfer protection: '+name);
  klass('holo-protos','Holoholo.Rpc.ReleasePokemonOutProto').field('Status');
  checkPromiseLayout(klass('holo-protos','Holoholo.Rpc.ReleasePokemonOutProto'));
  transferLedger=new transferPolicy.SessionTransferLedger(config.sessionId,config.maxTransfers);
  // No collection scan or restored queue: only issued, successful captures can grant scope.
  emit('transfer_scope',{mode:'CURRENT_SESSION_CAPTURE_RESPONSES',sessionId:config.sessionId,
    existingCollectionScan:false,queuePersisted:false,queueEmpty:true,transferRequestsSent:0});
  emit('transfer_preflight',{passed:true,keepShiny:config.keepShiny,keepHundo:config.keepHundo,
    maxTransfers:config.maxTransfers,transferRequestsSent:0});
}
function readTransferData(pokemon:Obj) {
  const id=pokemon.field<UInt64>('id_').value.toString();
  const fort=pokemon.field<Il2Cpp.String>('deployedFortId_').value;
  const station=pokemon.field<Il2Cpp.String>('deployedStationId_').value;
  const limited=pokemon.field<Il2Cpp.String>('limitedPokemonIdentifier_').value;
  return {id,dexId:number(pokemon.field('pokemonId_').value),
    attack:number(pokemon.field('individualAttack_').value),defense:number(pokemon.field('individualDefense_').value),
    stamina:number(pokemon.field('individualStamina_').value),shiny:pokemon.method<boolean>('get_IsShinyPokemon',0).invoke(),
    favorite:pokemon.field<boolean>('favorite_').value,egg:pokemon.field<boolean>('isEgg_').value,
    deployed:!!(fort.content||station.content),fusion:pokemon.field<boolean>('isFusion_').value,
    limited:!!limited.content,
    buddy:findService('Niantic.Holoholo.Buddy.IBuddyPokemonService').method<boolean>('GetIsBuddy',1).invoke(new UInt64(id)),
    pokemonClass:number(pokemon.method('GetClass',1).invoke(findService('IGameMasterData','holo-protos')))};
}
function tryTransfer(id:string,encounterId:string) {
  if(transferSync||pending)throw Error('Overlapping transfer not allowed');
  if(config.readOnly||!config.transferEnabled||!transferLedger.authorize(id,encounterId)) {
    emit('transfer_skipped',{id,encounterId,reason:'scope_or_limit'});
    transferLedger?.finish(id,false);
    if(!transferLimitReported&&config.maxTransfers>0&&stats.transferred>=config.maxTransfers) {
      transferLimitReported=true;emit('transfer_limit',{limit:config.maxTransfers,capturesContinue:true});
    }
    return;
  }
  enterStage('transfer:revalidate');
  const bag=findService('Niantic.Holoholo.IPokemonBag');
  // Re-fetch and re-read immediately before releasing: honor a new favorite/buddy.
  const value=bag.method<Obj>('GetPokemon',1).invoke(new UInt64(id));
  if(!(value instanceof Il2Cpp.Object)||value.isNull()) {
    transferLedger.finish(id,false);emit('transfer_skipped',{id,encounterId,reason:'owned_pokemon_missing'});return;
  }
  const ref=retain(value,'transfer:revalidated-pokemon');
  try {
    const pokemon=heldObject(ref,'Transfer.Pokemon');
    if(pokemon.class.fullName!=='Holoholo.Rpc.PokemonProto'||pokemon.field<UInt64>('id_').value.toString()!==id)
      throw Error('Transfer identity changed; not transferred');
    const data=readTransferData(pokemon);
    const verdict=transferPolicy.transferVerdict(data,config);
    emit('transfer_decision',{origin:'session',encounterId,...data,...verdict,
      keepShiny:config.keepShiny,keepHundo:config.keepHundo});
    if(!verdict.transferAllowed) {transferLedger.finish(id,false);return;}
    if(!active(true))throw Error('Transfer paused by focus/lease; not retried');
    // Consume the single-session grant BEFORE calling the game. No retries on an
    // exception, unknown status, timeout or lost connection, even after restart.
    transferLedger.begin(id,encounterId);
    transferDispatch={id,encounterId};
    emit('transfer_request',{id,encounterId,scope:'current_session',preexistingCollectionTargeted:false});
    reserve('transfer',object(observed('ReleasePokemon',()=>bag.method<Obj>('ReleasePokemon',1).invoke(pokemon)),
      'PokemonBag.ReleasePokemon'),id,[pokemon]);
  } finally {ref.free();}
}
function pollTransferSync(now:number):boolean {
  const sync=transferSync;if(!sync)return false;
  if(now<sync.nextRead)return false;
  const result=findService('Niantic.Holoholo.IPokemonBag').method<Obj>('GetPokemon',1).invoke(new UInt64(sync.id));
  if(result instanceof Il2Cpp.Object&&result.isNull()) {
    transferLedger.finish(sync.id,true);stats.transferred++;storageCache=null;transferSync=null;transferDispatch=null;
    emit('transfer',{id:sync.id,encounterId:sync.encounterId,confirmed:true,serverStatus:1,inventoryAbsent:true});
    emit('stats',statistics());return false;
  }
  if(!(result instanceof Il2Cpp.Object)||now-sync.started>=15000)
    throw Error('Transfer accepted but inventory not reconciled; not repeated');
  sync.nextRead=now+200;return false;
}
function readLocation(value:any,source:string):Location {
  enterStage('location:'+source);
  // Niantic.LatLng is a struct, not an Il2Cpp.Object. Read its fields via the
  // bridge's ValueType, then copy scalars instead of retaining native storage.
  let location:Il2Cpp.ValueType;
  if(value instanceof Il2Cpp.ValueType)location=value;
  else if(value instanceof Il2Cpp.Object && !value.isNull() && value.class.isValueType)location=value.unbox();
  else throw Error(source+': expected a Niantic.LatLng struct; got '+valueTypeName(value));
  if(location.type.name!=='Niantic.LatLng')throw Error(source+': incompatible struct '+location.type.name);
  const latitude=number(location.field('Latitude').value),longitude=number(location.field('Longitude').value);
  if(!Number.isFinite(latitude)||!Number.isFinite(longitude)||Math.abs(latitude)>90||Math.abs(longitude)>180)
    throw Error(source+': coordinates non-finite or out of range');
  return {latitude,longitude};
}
function playerLocation():Location {
  const avatar=findService('Niantic.Holoholo.Avatar.IMapAvatar');
  enterStage('location:avatar');
  return readLocation(avatar.method<Il2Cpp.ValueType>('get_Location',0).invoke(),'avatar');
}
function distance(a:Location,b:Location):number {
  return 111320*Math.hypot(b.latitude-a.latitude,
    (b.longitude-a.longitude)*Math.cos(a.latitude*Math.PI/180));
}
function reserve(kind:Pending['kind'],promise:Obj,id:string,held:Obj[]=[],extra:Partial<Pending>={}) {
  // No native callbacks are installed in game promises: poll their runtime
  // fields on the Unity thread. Stopping cannot leave dangling JS delegates.
  for (const name of ['completeCalled','errorCalled','completedValue']) {
    const f=promise.field(name);
    if (f.offset<Il2Cpp.Object.headerSize) throw Error('Incompatible promise layout: '+name);
  }
  const promiseHandle=retain(promise,'promise:'+kind);
  if(config.sessionAccounting&&!Number.isSafeInteger(requestSequence+1))throw Error('Action sequence exhausted');
  pending={kind,promise:promiseHandle,handles:[promiseHandle,...held.map((x,i)=>retain(x,kind+':argument:'+i))],
    started:Date.now(),id,...extra,sequence:config.sessionAccounting?++requestSequence:undefined};
  nextAction=pending.started+config.actionDelayMs;
  lastRequestAt=pending.started;operationalState='waiting_'+kind;
  emit('pending',{kind,id});
  trace('promise:reserved',{kind,id,promiseClass:promise.class.fullName,address:promise.handle.toString(),
    offsets:Object.fromEntries(['completeCalled','errorCalled','completedValue'].map(name=>[name,promise.field(name).offset]))});
}
function sendRpc(id:number,request:Obj,responseName:string):Obj {
  const handler=findService('Niantic.Holoholo.Rpc.IRpcHandler');
  const response=klass('holo-protos','Holoholo.Rpc.'+responseName);
  const method=handler.method<Obj>('SendRpc',5).inflate(request.class,response);
  if(method.virtualAddress.isNull()) throw Error('Generic SendRpc unresolved');
    // The game's facade owns serialization and callback lifetime. Inventory
    // reconciliation is checked separately; never assume a local count changed.
  enterStage('rpc:'+responseName);
  return object(method.invoke(id,request,1000,10000,false),'SendRpc('+responseName+')');
}
function currentWindow():boolean {
  if (!javaReady) return false;
  const now=Date.now(); if(now-lastWindowCheck<200) return windowFocused;
  lastWindowCheck=now;
  try { Java.performNow(()=>{const a=activity.currentActivity.value; windowFocused=a!==null && !!a.hasWindowFocus();}); }
  catch {windowFocused=false;}
  return windowFocused;
}
function active(forceSession=false):boolean {
  const shadeAllowed=config.notificationShadeSupport && notificationShade && startupGate?.active===true;
  const permitted=policy.leaseValid(Date.now(),lastBeat,allowed,true,stopped) && (currentWindow() || shadeAllowed);
  if(permitted)validateSession(forceSession);
  return permitted;
}
function releaseTarget() {target?.free();target=null;throws=0;targetSelectedAt=0;berryCycle=null;}
function skipTarget(reason:string,distanceMeters:number|null=null) {
  if(!target)return;
  const pokemon=inspectHeld(target,'Objetivo descartado');
  const id=pokemon.method<UInt64>('get_EncounterId',0).invoke().toString();
  seen.set(id,Date.now()+LOST_TARGET_COOLDOWN_MS);skippedTargets++;
  operationalState='scanning';
  emit('target_skipped',{id,reason,distanceMeters,captureRequestsSent:throws,
    ageMs:Date.now()-targetSelectedAt,retryAfterMs:LOST_TARGET_COOLDOWN_MS});
  releaseTarget();
}
function stop(reason:string) {
  if(stopped) return; stopped=true; allowed=false;
  trace('stop:before',{reason,pendingKind:pending?.kind??null,ownedReferences:gc?.count??0});
  if(transferDispatch)emit('transfer_pending_at_stop',{
    ...transferDispatch,serverAck:!!transferSync,retryAllowed:false});
  // Only observers are detached. Already sent game requests are not cancelled
  // or reported as undone; polling holds no custom game callbacks.
  let observerError='';
  try {hook?.detach();}catch(error){observerError=String(error);}hook=null;
  if(uiAdapter) {
    try {
      const result=uiAdapter.close();uiAdapter=null;emit('ui_interruption_cleanup',result);
      if(!result.passed)observerError+='; GUI cleanup: '+result.errors.join('; ');
    }catch(error){observerError+='; GUI cleanup: '+String(error);}
  }
  let locationCleanupError='';
  if(locationAdapter){
    try {
      const result=locationAdapter.close();locationAdapter=null;
      emit('native_location_cleanup',result);
      if(!result.passed)locationCleanupError=(result.errors??['JNI cleanup unconfirmed']).join('; ');
    }catch(error){locationCleanupError=String(error);emit('native_location_cleanup',{passed:false,errors:[locationCleanupError]});}
  }
  const cleanup=gc?.close()??{passed:true,remaining:0,errors:[]};
  if(observerError){cleanup.passed=false;cleanup.errors.push(observerError);}
  if(locationCleanupError){cleanup.passed=false;cleanup.errors.push(locationCleanupError);}
  pending=null;target=null;throws=0;berryCycle=null;services.clear();containerHandle=null;container=null;
  for(const [id,task] of ownedReads)emit('owned_read_pending_at_stop',{ownedId:id,encounterId:task.encounterId,transferAllowed:false});
  ownedReads.clear();
  transferSync=null;transferDispatch=null;transferLedger?.close();
  emit('gc_cleanup',{...cleanup,handleBits:64});
  if(!cleanup.passed)emit('error',{message:'Reference release not confirmed',stage:'gc:cleanup',errors:cleanup.errors});
  // Stop acknowledgement is sent AFTER releasing owned references.
  emit('stopped',{reason,limitStopKind,cleanupPassed:cleanup.passed,...statistics()});
}
function poll() {
  const p=pending;if(!p) return;
  enterStage('response:'+p.kind);
  const promise=inspectHeld(p.promise,'Retained promise '+p.kind);
  const failure=observed('promise:error-flags',()=>!!promise.field<boolean>('errorCalled').value || !!promise.tryField<boolean>('statusErrorCalled')?.value);
  if(failure) {
    const reason=String(promise.tryField('errorValue')?.value??promise.tryField('errorStatus')?.value??'');
    if(p.kind==='berry') {
      berryCycle?.reject();
      emit('berry_rejected',{id:p.id,item:p.item,reason,elapsedMs:Date.now()-p.started,
        retryAllowed:false,captureMayContinue:true,berryCountAdded:0});
      pending=null;p.handles.forEach(h=>h.free());
      nextAction=Math.max(nextAction,p.started+config.actionDelayMs);return;
    }
    // A despawned/unavailable encounter is not a reason to retry the same object
    // forever or stop farming all other nearby objects.
    if(p.kind==='encounter') {
      emit('encounter_rejected',{id:p.id,reason});seen.set(p.id,Date.now()+LOST_TARGET_COOLDOWN_MS);
      if(config.encounterOnly){emit('probe_result',{mode:'ENCOUNTER_ONLY',outcome:'rejected',id:p.id,reason});stop('Encounter probe rejected; no ball thrown');return;}
      pending=null;skipTarget('encounter_rejected');p.handles.forEach(h=>h.free());
      nextAction=Math.max(nextAction,p.started+config.actionDelayMs);return;
    }
    if(p.kind==='capture') {
      if(config.transferEnabled) {
        if(!p.captureRequest)throw Error('Catch has no session request');
        transferLedger.abandonCapture(p.captureRequest);
      }
      emit('capture_rejected',{id:p.id,reason,attempts:throws,retryAllowed:false,
        captureCountAdded:0,transferAllowed:false});
      pending=null;skipTarget('capture_rejected');seen.set(p.id,Date.now()+300000);
      p.handles.forEach(h=>h.free());
      nextAction=Math.max(nextAction,p.started+config.actionDelayMs);return;
    }
    throw Error('Response rejected: '+p.kind+' '+reason);
  }
  const completed=observed('promise:complete-flag',()=>promise.field<boolean>('completeCalled').value);
  trace('promise:state',{kind:p.kind,id:p.id,completed,failure,ageMs:Date.now()-p.started});
  if(!completed) {
    if(Date.now()-p.started>18000) throw Error('Timed out waiting for '+p.kind+'; not resent');
    return;
  }
  const result=object(observed('promise:completed-value',()=>promise.field<Obj>('completedValue').value),'Promesa '+p.kind+'.completedValue');
  // Hold the result while releasing the promise and interpreting primitive data.
  const held=retain(result,'response:'+p.kind);pending=null;
  try {
    if(p.kind==='encounter') {
      if(result.class.fullName!=='Niantic.Holoholo.Map.PokemonEncounterResponse')
        throw Error('Incompatible encounter response class; no ball thrown');
      emit('encounter',{id:p.id});
      if(berryProfileReady) {
        const initialItem=number(result.field('InitialActiveItem').value);
        berryCycle=new berryPolicy.BerryCycle(initialItem);
        emit('berry_encounter',{id:p.id,activeItem:initialItem,berryRequestsSent:0});
      }
      if(config.encounterOnly){emit('probe_result',{mode:'ENCOUNTER_ONLY',outcome:'response_received',id:p.id,
        responseClass:result.class.fullName,captureRequestsSent:0});stop('Encounter responded; probe finished without throwing a ball');return;}
    }
    else if(p.kind==='berry') {
      if(result.class.fullName!=='Holoholo.Rpc.UseItemEncounterOutProto')throw Error('Incompatible berry response class');
      if(!berryCycle)throw Error('Berry response has no encounter state');
      const status=number(result.field('Status').value), activeItem=number(result.field('ActiveItem').value);
      const verdict=berryCycle.settle(status,activeItem,p.item);
      if(verdict.consumed)stats.berriesUsed++;
      emit('berry_result',{id:p.id,item:p.item,status,activeItem,...verdict,elapsedMs:Date.now()-p.started,
        confirmed:verdict.consumed,berriesUsed:stats.berriesUsed});
      if(verdict.action==='skip')skipTarget('berry_encounter_completed');
    }
    else if(p.kind==='capture') {
      if(result.class.fullName!=='Holoholo.Rpc.CatchPokemonOutProto')throw Error('Incompatible catch response class');
      const status=number(result.field('Status').value), decision=policy.catchDecision(status,throws,config.maxThrows);
      emit('capture',{id:p.id,status,decision,requestedThrowQuality:config.throwQuality});
      berryCycle?.capture(status);
      let scopeAccepted=false;
      if(config.transferEnabled) {
        if(!p.captureRequest)throw Error('Catch has no session request');
        const proof={...p.captureRequest,status,ownedId:status===1?result.field<UInt64>('CapturedPokemonId').value.toString():null,
          completedAt:Date.now(),freshPromise:p.freshPromise,responseClass:result.class.fullName};
        const verdict=transferLedger.settleCapture(proof);scopeAccepted=verdict.accepted;
        emit('capture_scope_result',{...proof,...verdict});
      }
      if(decision==='caught') {stats.caught++;storageCache=null;if(config.inspectOwned)queueOwnedRead(result,p.id,scopeAccepted);
        accountReward(p,result,status);}
      if(decision==='fled') stats.fled++;
      if(decision!=='retry') {
        if(['rejected','replacement','exhausted'].includes(decision))skipTarget('capture_'+decision);
        else {seen.set(p.id,Date.now()+300000);releaseTarget();}
      }
      if(decision==='error') throw Error('Unsupported catch result: '+status);
    } else if(p.kind==='spin') {
      const status=number(result.field('Result').value);
      spinObserver.response(Date.now(),p.id,status);
      if(status===1) {stats.spun++;accountReward(p,result,status);}
      stops.set(p.id,Date.now()+(status===1||status===3?300000:30000));
      emit('spin',{id:p.id,status,confirmed:status===1});
      if(status===4) {nextInventory=0;nextSpin=Date.now()+10000;emit('bag_full',{retryAfterMs:10000,cleanEnabled:config.cleanBag});}
      if(status===5) throw Error('Daily spin limit');
    } else if(p.kind==='transfer') {
      if(result.class.fullName!=='Holoholo.Rpc.ReleasePokemonOutProto')throw Error('Incompatible transfer response class; not repeated');
      const status=number(result.field('Status').value);
      emit('transfer_response',{id:p.id,status,confirmed:false});
      if(status===1) {
        const encounterId=transferLedger.captures.get(p.id)?.encounterId;
        if(!encounterId)throw Error('Transfer has no session authorization');
        transferSync={id:p.id,encounterId,started:Date.now(),nextRead:Date.now()+200};
      } else if([2,3,4,5,6,7].includes(status)) {
        transferLedger.finish(p.id,false);transferDispatch=null;
        emit('transfer_skipped',{id:p.id,reason:'server_rejected',status,retryAllowed:false});
      } else throw Error('Ambiguous transfer response: '+status+'; not resent');
    } else {
      if(result.class.fullName!=='Holoholo.Rpc.RecycleItemOutProto')throw Error('Incompatible discard response class; not repeated');
      const status=number(result.field('Result').value), remaining=number(result.field('NewCount').value);
      if(config.sessionAccounting) {
        const verdict=discardResponse.assess(status,p.before,p.count,remaining,p.keep);
        emit('discard_response',{sequence:p.sequence,name:p.id,item:p.item,result:status,newCount:remaining,
          before:p.before,requested:p.count,keep:p.keep,expectedMaximumRemaining:p.before!-p.count!,
          ...verdict,retryAllowed:false});
        if(verdict.action==='fatal')throw Error('Unsafe discard response: '+verdict.reason+'; not repeated');
        if(verdict.action==='pause') {
          cleanupPaused=true;cleanupPauseReason=verdict.reason;startupCleanupPending=false;
          emit('cleanup_paused',{reason:verdict.reason,name:p.id,result:status,newCount:remaining,
            requested:p.count,before:p.before,keep:p.keep,scope:'ALL_RECYCLING_UNTIL_NEW_SESSION',
            retryAllowed:false,discardCountAdded:0,capturesMayContinue:true,spinsMayContinue:true,
            locationTreatmentKept:!!locationAdapter});
          // The request has settled and will not be sent again. No pending
          // inventory reconciliation or startup loop is left to block farming.
          nextAction=Math.max(nextAction,p.started+config.actionDelayMs);return;
        }
      }
      if(!policy.discardConfirmed(status,p.before,p.count,remaining,p.keep))throw Error('Discard unconfirmed or retention minimum breached; not repeated');
      stats.discarded+=p.count!;emit('discard',{name:p.id,item:p.item,count:p.count,remaining,confirmed:true});
      inventorySync={name:p.id,remaining,started:Date.now(),keep:p.keep!};
      nextInventory=Date.now()+1500;
    }
    // Minimum spacing is measured from the request, not added again after its reply.
    if(reportSessionLimits())return;
    nextAction=Math.max(nextAction,p.started+config.actionDelayMs);
  } finally {held.free();p.handles.forEach(h=>h.free());}
}
function startCapture(counts:Record<string,number>):boolean {
  if(!captureAllowed()){releaseTarget();return false;}
  if(config.encounterOnly)throw Error('Encounter probe does not allow TryCapture');
  enterStage('capture:target');
  const pokemon=inspectHeld(target!,'Retained Pokémon');
  if(pokemon.method<boolean>('get_Destroyed',0).invoke()) {skipTarget('destroyed');return false;}
  // A route may move the avatar while encounter/retry responses are pending.
  // Never send a throw against the old position; re-read before EVERY throw.
  const range=distance(playerLocation(),readLocation(pokemon.method<Il2Cpp.ValueType>('get_Location',0).invoke(),'pokemon'));
  if(!Number.isFinite(range)||range>CAPTURE_RANGE_METERS){skipTarget('out_of_range',range);return false;}
  if(pokemonStorage().pokemonBagFull){skipTarget('pokemon_storage_full',range);return false;}
  const ball=['PokeBall','GreatBall','UltraBall'].find(name=>counts[name]>0);
  if(!ball){releaseTarget();emit('no_balls');return false;}
  if(berryProfileReady&&berryCycle) {
    const name=berryCycle.choose(config.berryOrder,counts);
    if(name) {
      if(!active(true))return true;
      const berryItem=klass('holo-protos','Holoholo.Rpc.Item').field(name).value;
      const id=pokemon.method<UInt64>('get_EncounterId',0).invoke().toString();
      enterStage('capture:UseItemForEncounter');
      berryCycle.request();
      const promise=object(observed('UseItemForEncounter',()=>findService('Niantic.Holoholo.IItemBag')
        .method<Obj>('UseItemForEncounter',2).invoke(berryItem,pokemon)),'IItemBag.UseItemForEncounter');
      emit('berry_requested',{id,name,item:berryPolicy.ITEMS[name],before:counts[name],attempt:throws+1});
      reserve('berry',promise,id,[pokemon],{item:berryPolicy.ITEMS[name]});return true;
    }
  }
  const item=klass('holo-protos','Holoholo.Rpc.Item').field(ball).value;
  const data=klass('holo-game','Niantic.Holoholo.Encounter.PokeballThrow').method<Il2Cpp.ValueType>('MissedThrow',1).invoke(item);
  const profile=throwProfile.profile(config.throwQuality);
  data.field('Missed').value=false;data.field('HitBullseye').value=profile.hitBullseye;
  data.field('Spinning').value=true;data.field('ReticleSize').value=profile.reticleSize;
  const ar=proto('ARPlusEncounterValuesProto');
  // Conservative AR defaults, not fabricated expert/proximity bonuses.
  const id=pokemon.method<UInt64>('get_EncounterId',0).invoke().toString();
  if(!active(true))return true;
  enterStage('capture:TryCapture');
  const captureRequest:CaptureRequest|undefined=config.transferEnabled?transferLedger.issueCapture(id,Date.now()):undefined;
  emit('throw_requested',{id,quality:config.throwQuality,grade:profile.grade,reticleSize:profile.reticleSize,
    hitBullseye:profile.hitBullseye,curveball:true,attempt:throws+1});
  if(captureRequest)emit('capture_request',{...captureRequest,method:'MapPokemon.TryCapture'});
  const promise=object(observed('TryCapture',()=>pokemon.method<Obj>('TryCapture',2).invoke(data,ar)),'MapPokemon.TryCapture');
  // Completed-on-return may be legitimate but could be a cached response. Do not
  // authorize irreversible transfers without observing a fresh pending promise.
  const freshPromise=config.transferEnabled?!promise.field<boolean>('completeCalled').value
    &&!promise.field<boolean>('errorCalled').value&&!promise.tryField<boolean>('statusErrorCalled')?.value:undefined;
  throws++;reserve('capture',promise,id,[ar,pokemon],{captureRequest,freshPromise});return true;
}
function nextPokemon(directory:Obj) {
  if(!captureAllowed())return;
  if(config.encounterOnly&&encounterAttempted)return;
  const now=Date.now();
  if(seen.size>=4096){operationalState='encounter_cache_capacity';return;}
  operationalState='scanning';
  const location=playerLocation();
  enterStage('capture:list');
  const scan={scanned:0,destroyed:0,cooldown:0,unsupported:0,outOfRange:0,eligible:0};
  const reportScan=now>=nextScanReport;
  const visible:{id:string;distanceMeters:number}[]=[];
  let nearest:HeldReference64|null=null,nearestRange=Infinity,nearestId='';
  try {for(const pokemon of enumerate(object(directory.method<Obj>('get_AllPokemon',0).invoke(),'MapDirectory.AllPokemon'))) {
    scan.scanned++;
    if(pokemon.method<boolean>('get_Destroyed',0).invoke()){scan.destroyed++;continue;}
    const id=pokemon.method<UInt64>('get_EncounterId',0).invoke().toString();
    const cooldown=(seen.get(id)??0)>now;
    if(cooldown&&!reportScan){scan.cooldown++;continue;}
    // Only ordinary world encounters for this version, not raids/grunts/rewards.
    if(!['WildMapPokemon','IncenseMapPokemon','TroyDiskMapPlacePokemon','DailyEncounterMapPokemon','PhotobombingMapPokemon'].includes(pokemon.class.name)){scan.unsupported++;continue;}
    const range=distance(location,readLocation(pokemon.method<Il2Cpp.ValueType>('get_Location',0).invoke(),'pokemon'));
    visible.push({id,distanceMeters:range});
    if(cooldown){scan.cooldown++;continue;}
    if(!Number.isFinite(range)||range>CAPTURE_RANGE_METERS){scan.outOfRange++;continue;}
    scan.eligible++;
    if(range<nearestRange){const held=retain(pokemon,'capture:candidate');nearest?.free();nearest=held;nearestRange=range;nearestId=id;}
  }
  lastMapScan=scan;
  if(reportScan){nextScanReport=now+5000;emit('map_scan',{...scan,...mapObserver.observe(now,location,visible),rangeMeters:CAPTURE_RANGE_METERS,
    closestEligibleMeters:nearest?nearestRange:null,selection:'nearest_current_position',scanLimit:256});}
  if(nearest) {
    if(!active(true))return;
    const pokemon=heldObject(nearest,'Catch candidate');
    target=nearest;nearest=null;throws=0;targetSelectedAt=Date.now();seen.set(nearestId,now+30000);
    berryCycle=null;
    // Final recheck also covers a candidate removed during the map scan.
    if(pokemon.method<boolean>('get_Destroyed',0).invoke()){skipTarget('destroyed');return;}
    const range=distance(playerLocation(),readLocation(pokemon.method<Il2Cpp.ValueType>('get_Location',0).invoke(),'pokemon'));
    if(!Number.isFinite(range)||range>CAPTURE_RANGE_METERS){skipTarget('out_of_range',range);return;}
    trace('encounter:selected',{id:nearestId,class:pokemon.class.fullName,address:pokemon.handle.toString(),distanceMeters:range});
    emit('target_selected',{id:nearestId,distanceMeters:range,eligible:scan.eligible,selection:'nearest_current_position'});
    enterStage('capture:SendEncounterRequest');
    encounterAttempted=true;
    reserve('encounter',object(observed('SendEncounterRequest',()=>pokemon.method<Obj>('SendEncounterRequest',0).invoke()),'MapPokemon.SendEncounterRequest'),nearestId,[pokemon]);return;
  }
  operationalState='no_eligible_nearby_pokemon';
  } finally {nearest?.free();}
}
function spin(directory:Obj) {
  if(!spinAllowed())return;
  const now=Date.now();nextSpin=now+3000;
  const full=findService('Niantic.Holoholo.Services.IItemBagService').method<boolean>('get_IsFull',0).invoke();
  if(full) {
    spinObserver.observe(now,{blocked:'item_bag_full'},emit);
    spinBagBlocked=true;nextSpin=now+5000;
    if(now>=nextSpinBlockedReport){nextSpinBlockedReport=now+30000;
      emit('spin_paused',{reason:'item_bag_full',retryAfterMs:5000,capturesMayContinue:true,cleanEnabled:config.cleanBag});}
    return;
  }
  if(spinBagBlocked){emit('spin_resumed',{previousReason:'item_bag_full'});spinBagBlocked=false;}
  if(stops.size>=4096){operationalState='stop_cache_capacity';spinObserver.observe(now,{blocked:'stop_cache_capacity'},emit);return;}
  const location=playerLocation();
  const lat=location.latitude,lng=location.longitude;
  enterStage('spin:list');
  const stopObjects=enumerate(object(directory.method<Obj>('get_ActivePokestops',0).invoke(),'MapDirectory.ActivePokestops'));
  const scan={listed:stopObjects.length,visited:0,cooldown:0,outOfRange:0,invalidDistance:0,
    scanLimitReached:stopObjects.length===256,complete:false,closestCheckedMeters:null as number|null,
    earliestCooldownMs:null as number|null,selectedId:'',selectedDistanceMeters:null as number|null};
  for (const stopObject of stopObjects) {
    scan.visited++;
    const id=(stopObject.method<Il2Cpp.String>('get_Id',0).invoke()).content!;
    const until=stops.get(id)??0;
    if(until>now) {scan.cooldown++;scan.earliestCooldownMs=Math.min(scan.earliestCooldownMs??Infinity,until-now);continue;}
    const stopLocation=readLocation(stopObject.method<Il2Cpp.ValueType>('get_Location',0).invoke(),'pokestop');
    const stopLat=stopLocation.latitude,stopLng=stopLocation.longitude;
    const dist=distance(location,stopLocation);
    if(!Number.isFinite(dist)){scan.invalidDistance++;continue;}
    scan.closestCheckedMeters=Math.min(scan.closestCheckedMeters??Infinity,dist);
    if(dist>80){scan.outOfRange++;continue;}
    const request=proto('FortSearchProto');
    field(request,'Id',Il2Cpp.string(id));field(request,'PlayerLatDegrees',lat);field(request,'PlayerLngDegrees',lng);
    field(request,'FortLatDegrees',stopLat);field(request,'FortLngDegrees',stopLng);
    // Optional ad/quest fields are left at game schema defaults.
    if(!active(true))return;
    reserve('spin',sendRpc(101,request,'FortSearchOutProto'),id,[request]);
    scan.selectedId=id;scan.selectedDistanceMeters=dist;spinObserver.observe(now,scan,emit);return;
  }
  scan.complete=true;spinObserver.observe(now,scan,emit);
}
// true: a surplus still needs processing; false: at floors or safety quota reached.
function discard(counts:Record<string,number>,bagFull:boolean):boolean {
  if(cleanupPaused)return false;
  enterStage('discard:plan');
  const remaining=config.maxDiscardedItems===0?50:Math.max(0,config.maxDiscardedItems-stats.discarded);
  const plan=remaining?policy.discardPlan(config.discard,counts,Math.min(50,remaining)):null;
  const summary=JSON.stringify({counts,bagFull,remaining,plan});
  if(summary!==lastInventorySummary||Date.now()>=nextInventoryReport) {
    lastInventorySummary=summary;nextInventoryReport=Date.now()+30000;
    emit('inventory_check',{cleanupTrigger:policy.CLEANUP_TRIGGER,bagFull,counts,limits:config.discard,planned:plan,
      maxDiscardedItems:config.maxDiscardedItems,quotaRemaining:config.maxDiscardedItems===0?null:remaining,
      startup:startupCleanupPending,status:!remaining?'quota_reached':plan?'surplus':'within_limits'});
  }
  if(!remaining){if(!discardLimitReported){discardLimitReported=true;emit('discard_limit',{
    maximum:config.maxDiscardedItems,discarded:stats.discarded,capturesContinue:true});}return false;}
  if(!plan){
    if(bagFull&&Date.now()>=nextBlockedReport){nextBlockedReport=Date.now()+30000;
      emit('bag_cleanup_blocked',{reason:'no_configured_surplus',counts,limits:config.discard,transferRequestsSent:0});}
    // No surplus in a non-full bag must not keep pushing spin scheduling back.
    return false;
  }
  emit('discard_plan',{...plan,bagFull,cleanupTrigger:policy.CLEANUP_TRIGGER,startup:startupCleanupPending});
  const request=proto('RecycleItemProto');field(request,'Item',klass('holo-protos','Holoholo.Rpc.Item').field(plan.name).value);
  field(request,'Count',plan.count);
  if(!active(true))return true;
  reserve('discard',sendRpc(137,request,'RecycleItemOutProto'),plan.name,[request],
    {item:plan.item,count:plan.count,before:plan.before,keep:plan.keep});
  return true;
}
function checkPromiseLayout(response:Il2Cpp.Class,isRpc=false) {
  enterStage('preflight:promise:'+response.fullName);
  const type=isRpc?klass('holo-game','Niantic.Holoholo.Rpc.RpcPromise`1'):
    klass('Niantic.Promises','Niantic.Promises.Promise`1');
  const concrete=type.inflate(response);
  for(const name of ['completeCalled','errorCalled','completedValue',...(isRpc?['statusErrorCalled']:[])]) {
    if(concrete.field(name).offset<Il2Cpp.Object.headerSize)
      throw Error('Incompatible generic promise before sending actions: '+name);
  }
}
function berryPreflight(bag:Obj) {
  if(!config.autoCatch||!config.useBerries||config.encounterOnly)return;
  const method=bag.tryMethod('UseItemForEncounter',2);
  if(!method||method.virtualAddress.isNull()||method.parameters.length!==2
      ||method.parameters[0].type.name!=='Holoholo.Rpc.Item'
      ||method.parameters[1].type.name!=='Niantic.Holoholo.Map.IMapPokemon') {
    emit('berry_profile',{available:false,reason:'incompatible_method',captureMayContinue:true,
      parameterTypes:method?.parameters.map(parameter=>parameter.type.name)??[],
      returnType:method?.returnType.name??null,methodAddress:method?.virtualAddress.toString()??null});return;
  }
  const response=klass('holo-protos','Holoholo.Rpc.UseItemEncounterOutProto');
  const initial=klass('holo-game','Niantic.Holoholo.Map.PokemonEncounterResponse').field('InitialActiveItem');
  for(const field of [initial,response.field('Status'),response.field('ActiveItem')])
    if(field.isStatic||field.offset<Il2Cpp.Object.headerSize)throw Error('Incompatible berry response layout');
  checkPromiseLayout(response);
  berryProfileReady=true;
  emit('berry_profile',{available:true,priority:config.berryOrder,method:'IItemBag.UseItemForEncounter',
    parameterTypes:method.parameters.map(parameter=>parameter.type.name),returnType:method.returnType.name,
    initialActiveItemChecked:true,minimumIntervalMs:config.actionDelayMs,extraFixedWaitMs:0,
    cleanupLimitsPreserved:true,gameRequestsSent:0});
}
function preflight(directory:Obj,bag:Obj) {
  bindSession(directory);
  enterStage('preflight:inventory');
  const total=number(bag.method('GetTotalItems',0).invoke());
  if(!Number.isInteger(total)||total<0)throw Error('Invalid bag count');
  enterStage('preflight:pokemon');
  const pokemon=enumerate(object(directory.method<Obj>('get_AllPokemon',0).invoke(),'MapDirectory.AllPokemon'),32);
  enterStage('preflight:pokestops');
  const pokestops=enumerate(object(directory.method<Obj>('get_ActivePokestops',0).invoke(),'MapDirectory.ActivePokestops'),32);
  const location=playerLocation();
  emit('location_validated',{source:'avatar',latitude:location.latitude,longitude:location.longitude});
  if(config.autoCatch) {
    pokemonStorage();
    checkPromiseLayout(klass('holo-game','Niantic.Holoholo.Map.PokemonEncounterResponse'));
    checkPromiseLayout(klass('holo-protos','Holoholo.Rpc.CatchPokemonOutProto'));
  }
  if(config.autoSpin) {
    const request=klass('holo-protos','Holoholo.Rpc.FortSearchProto');
    for(const name of ['Id','PlayerLatDegrees','PlayerLngDegrees','FortLatDegrees','FortLngDegrees'])request.field(name);
    const method=findService('Niantic.Holoholo.Rpc.IRpcHandler').method('SendRpc',5)
      .inflate(request,klass('holo-protos','Holoholo.Rpc.FortSearchOutProto'));
    if(method.virtualAddress.isNull())throw Error('SendRpc/FortSearch unavailable');
    checkPromiseLayout(klass('holo-protos','Holoholo.Rpc.FortSearchOutProto'),true);
  }
  if(config.autoSpin||config.cleanBag)
    findService('Niantic.Holoholo.Services.IItemBagService').method<boolean>('get_IsFull',0).invoke();
  if(config.cleanBag) {
    const request=klass('holo-protos','Holoholo.Rpc.RecycleItemProto');
    request.field('Item');request.field('Count');
    const method=findService('Niantic.Holoholo.Rpc.IRpcHandler').method('SendRpc',5)
      .inflate(request,klass('holo-protos','Holoholo.Rpc.RecycleItemOutProto'));
    if(method.virtualAddress.isNull())throw Error('SendRpc/RecycleItem unavailable');
    checkPromiseLayout(klass('holo-protos','Holoholo.Rpc.RecycleItemOutProto'),true);
  }
  if(config.transferEnabled)transferPreflight();
  if(config.inspectOwned)inspectOwnedPreflight();
  berryPreflight(bag);
  rewardPreflight();
  const inventoryCounts=getInventory(true).counts;
  emit('inventory_plan',{counts:inventoryCounts,limits:config.discard,
    planned:policy.discardPlan(config.discard,inventoryCounts),cleanEnabled:config.cleanBag,cleanupTrigger:policy.CLEANUP_TRIGGER});
  enterStage('preflight:gc-references');
  if(!gc)throw Error('GCHandle64 missing in preflight');
  const avatar=findService('Niantic.Holoholo.Avatar.IMapAvatar');
  enterStage('preflight:gc-references');
  const probe=gc.probe(avatar.handle);
  const cachedReferences=gc.validateCached();
  emit('gc_validation',{passed:true,handleBits:64,cachedReferences,...probe});
  enterStage('preflight:complete');
  emit('preflight',{totalItems:total,pokemonVisible:pokemon.length,pokestopsVisible:pokestops.length,
    capture:config.autoCatch,spin:config.autoSpin,discard:config.cleanBag,inspectOwned:config.inspectOwned,
    transferEnabled:config.transferEnabled});
}
function tick() {
  const now=Date.now();if(stopped||inTick||now-lastTick<60)return;
  lastTick=now;inTick=true;unityThreadId=Process.getCurrentThreadId();
  try {
    validateSession();
    // Sweep independently of action returns: continuous RPC traffic cannot
    // bypass TTL cleanup indefinitely. Never evict unexpired cooldowns.
    if(now>=nextCacheSweep){runtimePolicy.pruneExpired(seen,now);runtimePolicy.pruneExpired(stops,now);nextCacheSweep=now+15000;}
    if(now>=nextTraceSummary&&repeatedStages.size){
      emit('trace_summary',{stageOccurrences:Object.fromEntries(repeatedStages),sampleIntervalMs:30000,
        actionTracesSampled:false,rawSystemLogsFiltered:false});repeatedStages.clear();nextTraceSummary=now+30000;
    }
    if(pending) {
      poll();if(stopped)return;
      if(pending){if(ready&&now>=nextHealth)reportHealth(now,getInventory().counts);return;}
    }
    if(!active()){if(startupGate&&!startupGate.active)startupGate.reset();return;}
    const directory=findService('Niantic.Holoholo.Map.IMapPlaceDirectoryService');
    const {counts,bag}=getInventory();
    if(!preflightStarted){preflight(directory,bag);preflightStarted=true;}
    if(!ready){ready=true;sessionReadyAt=now;stage='ready';
      if(config.automaticPreflight)startupGate=new startupPolicy.StartupGate(config.sessionId,sessionPlayerId);
      emit('ready',{mode:config.automaticPreflight?'PREFLIGHT':config.readOnly?'READ_ONLY':config.encounterOnly?'ENCOUNTER_ONLY':'INTERNAL',counts});}
    reportHealth(now,counts);
    if(now>=nextReport){emit('stats',statistics());nextReport=now+2000;}
    if(config.readOnly){
      if(config.diagnosticTrace&&now-readSampleAt>=500){
        readSampleAt=now;if(!readSampleStarted)readSampleStarted=now;
        const location=playerLocation(),cachedReferences=gc!.validateCached();
        emit('read_stability',{passed:true,samples:++readSamples,elapsedMs:now-readSampleStarted,
          cachedReferences,latitude:location.latitude,longitude:location.longitude});
        if(config.automaticPreflight){
          const proof=startupGate.observe(now,true,cachedReferences);
          if(proof)emit('preflight_validated',{...proof,pid:Process.id,automatic:true,singleAttach:true});
        }
      }
      if(config.automaticPreflight){
        if(activationRequest){
          const activationToken=activationRequest.token;
          validateSession(true);
          if(gc!.validateCached()<4)throw Error('Insufficient startup references');
          const accepted=active(true)&&startupGate.accept(activationRequest,now);
          activationRequest=null;
          if(!accepted){emit('activation_rejected',{reason:'invalid_expired_or_interrupted_proof',gameRequestsSent:0});return;}
          config.readOnly=false;startupCleanupPending=config.cleanBag;
          operationalState='starting';sessionReadyAt=now;nextHealth=0;
          emit('ready',{mode:'INTERNAL',counts,automaticPreflightPassed:true,singleAttach:true,
            playerId:sessionPlayerId,sessionId:config.sessionId,activationToken,gameRequestsSent:0});
        }
      }
      // First mutating action can only happen on a later Unity-thread tick.
      return;
    }
    pollTransferSync(now);
    // Item deletion must reconcile before ANY subsequent write, including a
    // queued Pokemon release. Reading counts never authorizes another deletion.
    if(inventorySync) {
      if(counts[inventorySync.name]<inventorySync.keep)throw Error('Inventory below retention minimum after discard; not repeated');
      if(counts[inventorySync.name]<=inventorySync.remaining)inventorySync=null;
      else {
        if(now-inventorySync.started>15000)throw Error('Bag did not update after discard; not repeated');
        return;
      }
    }
    // Normalize the selected types first, then maintain the same floors while
    // farming. No full-bag requirement; a safety quota may leave some surplus.
    if(startupCleanupPending) {
      if(now<nextAction||now<nextInventory)return;
      nextInventory=now+1000;
      const full=findService('Niantic.Holoholo.Services.IItemBagService').method<boolean>('get_IsFull',0).invoke();
      if(discard(counts,full))return;
      startupCleanupPending=false;
      emit('inventory_startup_complete',{cleanupTrigger:policy.CLEANUP_TRIGGER,
        reason:config.maxDiscardedItems>0&&stats.discarded>=config.maxDiscardedItems?'quota_reached':'within_limits',
        surplusRemains:policy.discardPlan(config.discard,counts)!==null,discarded:stats.discarded});
    }
    if(config.inspectOwned&&pollOwnedReads(now))return;
    if(now<nextAction)return;
    if(target&&startCapture(counts))return;
    // Fair scheduling: bag check and due spin cannot starve behind captures.
    if(config.cleanBag&&!cleanupPaused&&now>=nextInventory){
      nextInventory=now+1000;
      // Fullness is diagnostic only. Per-type excess is sufficient to recycle.
      const full=findService('Niantic.Holoholo.Services.IItemBagService').method<boolean>('get_IsFull',0).invoke();
      discard(counts,full);if(pending)return;
    }
    if(config.autoSpin&&spinAllowed()&&now>=nextSpin){spin(directory);if(pending)return;}
    // Bounded backpressure: never outrun the 64-entry inspection/transfer queue.
    if(config.autoCatch&&captureAllowed()&&ownedReads.size<16) {
      const storage=pokemonStorage(),full=storage.pokemonBagFull;
      const balls=['PokeBall','GreatBall','UltraBall'].some(name=>counts[name]>0);
      if(full||!balls){
        operationalState=full?'pokemon_storage_full':'no_balls';
        if(capturePauseReason!==operationalState)nextCaptureNotice=0;
        capturePauseReason=operationalState;
        if(now>=nextCaptureNotice){nextCaptureNotice=now+30000;
          emit('capture_paused',{reason:operationalState,...storage,preexistingPokemonPreserved:true,spinsAndCleanupMayContinue:true});}
      }else {
        if(capturePauseReason){emit('capture_resumed',{previousReason:capturePauseReason,...storage});capturePauseReason='';}
        nextPokemon(directory);
      }
    }
    if(!pending) nextAction=now+Math.min(300,config.actionDelayMs);
  } catch(e){fail(e);} finally{inTick=false;}
}
function init(_stage:string,parameters:any) {
  if(initialized)throw Error('Session already initialized');initialized=true;
  if(parameters.gamePackage!=='com.nianticlabs.pokemongo'||parameters.version!=='0.431.0')throw Error('Unsupported profile');
  config={gamePackage:parameters.gamePackage,autoCatch:!!parameters.autoCatch,autoSpin:!!parameters.autoSpin,
    cleanBag:!!parameters.cleanBag,readOnly:!!parameters.readOnly,...sessionLimits.read(parameters),
    maxThrows:parameters.maxThrows===0?0:Math.min(10,Math.max(1,parameters.maxThrows|0)),
    actionDelayMs:Math.min(3000,Math.max(100,parameters.actionDelayMs|0)),discard:parameters.discard??{},
    diagnosticTrace:parameters.diagnosticTrace===true,encounterOnly:parameters.encounterOnly===true,
    inspectOwned:parameters.inspectOwned===true||parameters.transferEnabled===true,
    transferEnabled:parameters.transferEnabled===true,keepShiny:parameters.keepShiny!==false,
    keepHundo:parameters.keepHundo!==false,maxTransfers:parameters.maxTransfers??0,sessionId:parameters.sessionId,
    maxDiscardedItems:parameters.maxDiscardedItems??0,expectedPlayerId:parameters.expectedPlayerId??'',
    automaticPreflight:parameters.automaticPreflight===true,
    sessionAccounting:parameters.edition==='ugphone'&&parameters.sessionAccounting===true,
    controlRecovery:parameters.edition==='ugphone'&&parameters.automaticPreflight===true&&parameters.controlRecovery===true,
    throwQuality:parameters.throwQuality??'GREAT',
    useBerries:parameters.useBerries===true,berryOrder:berryPolicy.order(parameters.berryOrder),
    skipInterruptions:parameters.skipInterruptions===true,
    notificationShadeSupport:parameters.edition==='ugphone'&&parameters.automaticPreflight===true
      &&parameters.controlRecovery===true&&parameters.notificationShadeSupport===true};
  throwProfile.profile(config.throwQuality);
  if(config.autoCatch&&config.useBerries&&config.berryOrder.length===0)throw Error('Select berries to use');
  if(config.automaticPreflight){
    if(config.readOnly||config.encounterOnly||typeof config.sessionId!=='string'||!/^[A-Za-z0-9_-]{8,96}$/.test(config.sessionId))
      throw Error('Automatic startup requires the full engine and session identity');
    config.readOnly=true;config.diagnosticTrace=true;
  }
  if(config.expectedPlayerId!==''&&!runtimePolicy.playerIdValid(config.expectedPlayerId))throw Error('Invalid preflight identity');
  if(!Number.isInteger(config.maxDiscardedItems)||config.maxDiscardedItems<0||config.maxDiscardedItems>10000)
    throw Error('Invalid item quota');
  if(config.cleanBag&&parameters.cleanupTrigger!==policy.CLEANUP_TRIGGER)
    throw Error('Cleanup requires EXCESS_FROM_START confirmation');
  startupCleanupPending=config.cleanBag&&!config.readOnly;
  for(const [name,keep] of Object.entries(config.discard))
    if(!Object.hasOwnProperty.call(policy.ITEMS,name)||!Number.isInteger(keep)||keep<0||keep>10000)
      throw Error('Cleanup item/minimum not allowed: '+name);
  if(config.transferEnabled&&(!config.autoCatch||config.encounterOnly||!Number.isInteger(config.maxTransfers)
    ||config.maxTransfers<0||config.maxTransfers>1000))throw Error('Transfers require catches, full engine and a valid quota');
  if(config.transferEnabled&&(typeof config.sessionId!=='string'||!/^[A-Za-z0-9_-]{8,96}$/.test(config.sessionId)))
    throw Error('Transfers require a unique session identifier');
  if(config.encounterOnly&&(!config.autoCatch||config.autoSpin||config.cleanBag))throw Error('Encounter probe requires catches only; no spins or cleanup');
  if(parameters.nativeLocationJNI===true){
    if(parameters.edition!=='ugphone'||!config.automaticPreflight)throw Error('JNI treatment restricted to the UGPhone edition');
    locationRequested=true;
  }
  lastBeat=Date.now();emit('attached',{pid:Process.id,arch:Process.arch,readOnly:config.readOnly});
  // CLI starts its stdin reader only AFTER init returns. Never block init on
  // Java/Unity readiness: otherwise no heartbeat or Stop can reach the agent.
  setImmediate(()=>{void setup();});
}
async function setup() {
  try {
    if(stopped)return;
    enterStage('setup:architecture');
    if(Process.arch!=='arm64')throw Error('ARM64 process required');
    if(locationRequested){
      enterStage('setup:native-location');
      locationAdapter=nativeLocation.installNativeLocation({Process,Module,Memory,NativeFunction,Interceptor,ptr},emit);
    }else if(!locationAdapter)emit('native_location_disabled',{enabled:false});
    enterStage('setup:java');
    await new Promise<void>((resolve,reject)=>Java.perform(()=>{
      try{activity=Java.use('com.unity3d.player.UnityPlayer');javaReady=true;resolve();}catch(e){reject(e);}
    }));
    enterStage('setup:il2cpp');
    await Il2Cpp.perform(()=>{
      if(stopped)return;
      const map=klass('holo-game','Niantic.Holoholo.Map.MapExploreState').method('get_IsActiveAndVisible',0);
      const capture=klass('holo-game','Niantic.Holoholo.Map.MapPokemon').method('TryCapture',2);
      if(capture.parameters[0].type.name!=='Niantic.Holoholo.Encounter.PokeballThrow')throw Error('Incompatible TryCapture signature');
      if(Il2Cpp.application.identifier!==config.gamePackage)throw Error('Incorrect process package');
      if(Il2Cpp.application.version!=='0.431.0')throw Error('Incompatible Unity application version');
      emit('profile',{unity:Il2Cpp.unityVersion,game:Il2Cpp.application.version,method:capture.name});
      // Width was verified against this exact game's ARM64 libil2cpp.so. Do
      // not guess it for other Unity versions or fall back to uint32 handles.
      if(Il2Cpp.unityVersion!=='6000.3.21f1')throw Error('GCHandle64 ABI statically validated only for Unity 6000.3.21f1');
      enterStage('setup:gc-abi');
      gc=new GcReferences64(createGcApi64(Il2Cpp.module));
      emit('gc_abi',{handleBits:64,pointerSize:Process.pointerSize,adapter:'rootcatch-gc64',unity:Il2Cpp.unityVersion});
      enterStage('setup:map-observer');
      hook=Interceptor.attach(map.virtualAddress,{onLeave(result){if(uiAdapter?.busy)return;uiAdapter?.drain();if(result.toInt32()!==0)tick();
        else if(startupGate&&!startupGate.active)startupGate.reset();}});
      if(config.skipInterruptions)uiAdapter=uiInterruptions.install({
        resolve:(name:string)=>klass('holo-game',name),object:(address:NativePointer)=>new Il2Cpp.Object(address),
        retain:(controller:Obj,label:string)=>retain(controller,label),read:(reference:HeldReference64)=>heldObject(reference,'GUI interruption'),
        number,isPointer:(value:any)=>value instanceof NativePointer,
        attach:(address:NativePointer,callbacks:InvocationListenerCallbacks)=>Interceptor.attach(address,callbacks),
        sameThread:()=>unityThreadId!==0&&Process.getCurrentThreadId()===unityThreadId,
        canObserve:()=>ready&&!stopped&&!config.readOnly&&!config.encounterOnly,
        canAct:()=>{
          if(!ready||stopped||config.readOnly||config.encounterOnly)return false;
          try {return active(true);}catch(error){fail(error);return false;}
        },now:()=>Date.now(),
      },emit);
      emit('observing',{message:'Waiting for visible map and game services'});
    },'free');
  }catch(e){fail(e);}
}
// Frida's CLI pipes stdin to this RPC export. No server/Internet permission,
// Magisk modules or automatic game launches are installed. The caller manages
// its own environment. The opt-in UGPhone JNI adapter does not change integrity
// checks or system settings; disappearance of Error 12 requires a device test.
export const enginePort={init,requestStop:stop,
bindLocationAdapter(adapter:any){if(initialized||locationAdapter)throw Error('Duplicate location adapter');locationAdapter=adapter;},
observeEvents(observer:(event:string)=>void){if(initialized)throw Error('Engine already started');eventObserver=observer;},
onFridaStdin(line:string){
  try{
    const command=JSON.parse(line);if(command.type==='stop'){stop(typeof command.reason==='string'?command.reason.slice(0,512):'Stopped by user');return;}
    if(command.type==='heartbeat'&&!stopped){
      if(config.controlRecovery){
        if(!Number.isSafeInteger(command.sequence)||command.sequence<=lastHeartbeatSequence
            ||!Number.isSafeInteger(command.sentAtElapsedMs)||command.sentAtElapsedMs<0)
          throw Error('Invalid control sequence');
        lastHeartbeatSequence=command.sequence;lastHeartbeatSentAt=command.sentAtElapsedMs;
      }
      lastBeat=Date.now();allowed=command.foreground===true;
      const shade=config.notificationShadeSupport && allowed && command.notificationShade===true;
      if(shade!==notificationShade)emit('notification_shade',{allowed:shade,
        underlyingGameConfirmedByHost:shade,sequencedControl:config.controlRecovery});
      notificationShade=shade;
      if(!allowed&&startupGate&&!startupGate.active)startupGate.reset();}
    if(command.type==='activate'&&!stopped){
      if(config.automaticPreflight&&config.readOnly)activationRequest=command;
      else emit('activation_rejected',{reason:'not_automatic_preflight'});
    }
  }catch{fail('Invalid control');}
},getFridaTerminalMode(){return 'cooked';}};
rpc.exports={init:enginePort.init,onFridaStdin:enginePort.onFridaStdin,getFridaTerminalMode:enginePort.getFridaTerminalMode};
setInterval(()=>{
  if(initialized&&!stopped){
    if(config?.controlRecovery){
      const status=controlWatchdog.state(Date.now(),lastBeat,true);
      const detail={heartbeatAgeMs:status.ageMs,lastBeatAt:lastBeat,lastHeartbeatSequence,lastHeartbeatSentAt,
        actionLeaseMs:controlWatchdog.ACTION_LEASE_MS,stopAfterMs:controlWatchdog.SILPH_STOP_MS,
        locationTreatmentRetained:!!locationAdapter,newActionsAllowed:false};
      if(status.phase==='expired'){
        emit('control_timeout',{...detail,reason:'controller_absent',connectionStopped:true});
        stop('SilphMod control absent for over 10 seconds');
      }else if(status.phase==='delayed'&&!controlDelayed){
        controlDelayed=true;emit('control_delay',{...detail,connectionStopped:false});
      }else if(status.phase==='healthy'&&controlDelayed){
        controlDelayed=false;emit('control_recovered',{...detail,foregroundAllowed:allowed,
          newActionsStillRequireFocusAndSessionValidation:true});
      }
    }else if(Date.now()-lastBeat>3000)stop('Engine control connection lost');
  }
  if(initialized&&!stopped&&locationAdapter)locationAdapter.summary(Date.now());
},500);
