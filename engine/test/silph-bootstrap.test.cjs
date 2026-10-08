const test=require('node:test'),assert=require('node:assert/strict');
const {createBootstrap,ENGINE_WARM_MS,LEASE_MS}=require('../src/silph-bootstrap.cjs');
const base={edition:'ugphone',automaticPreflight:true,readOnly:false,encounterOnly:false,
  gamePackage:'com.nianticlabs.pokemongo',version:'0.431.0',sessionId:'1791382837459_23162511',
  resolveError12:true,nativeLocationJNI:false,sdk:29,autoCatch:true,autoSpin:true,
  transferEnabled:false,cleanBag:false};
function harness(options={}) {
  let now=10000,closed=0,installed=0,loaded=0,libraries=true,observer,bound,params,loadResolve;
  const events=[],deferred=[],intervals=[],forwarded=[];
  const engine={
    init(stage,p){params=p;if(options.initError)throw Error('init rejected');},
    bindLocationAdapter(a){bound=a;},observeEvents(o){observer=o;},
    requestStop(reason){if(options.stopError)throw Error('stop error');bound?.close();observer('stopped');},
    onFridaStdin(line){forwarded.push(JSON.parse(line));},
  };
  const api=createBootstrap({Process:{id:65160,arch:'arm64',pointerSize:8},librariesReady:()=>libraries},
    (runtime,p,report)=>{
      installed++;
      assert.deepEqual(p,{protocol:2,session:base.sessionId,profile:'mock_treat',extended:false,backtrace:false,sdk:29});
      if(options.installError){const err=Error('install failed');err.traceRollback=options.rollback;throw err;}
      report('trace_targets_resolved',{mockMethods:['isFromMockProvider']});
      return {snapshot:()=>({hooksInstalled:3,coordinateMutation:false,systemMutation:false}),
        summary:()=>{},close:()=>{closed++;return {passed:!options.cleanupError,
          remainingHooks:options.cleanupError?1:0,remainingClassRefs:0,errors:options.cleanupError?['detach failed']:[]};}};
    },async()=>{loaded++;if(options.loadError)throw Error('load failed');
      if(options.pendingLoad)await new Promise(resolve=>loadResolve=resolve);return engine;},
    (event,detail)=>events.push({event,...detail}),()=>now,
    {setImmediate:fn=>deferred.push(fn),setInterval:fn=>{intervals.push(fn);return fn;},
      clearInterval:fn=>{const i=intervals.indexOf(fn);if(i>=0)intervals.splice(i,1);}});
  const beat=(extra={})=>api.onFridaStdin(JSON.stringify({type:'heartbeat',sessionId:base.sessionId,foreground:true,...extra}));
  async function advance(ms,heartbeat=true) {
    now+=ms;if(heartbeat)beat();for(const fn of [...intervals])fn();
    await new Promise(resolve=>setImmediate(resolve));
  }
  return {api,events,beat,advance,flush:()=>{while(deferred.length)deferred.shift()();},
    get closed(){return closed;},get installed(){return installed;},get loaded(){return loaded;},
    get params(){return params;},forwarded,setLibraries:value=>libraries=value,
    resolveLoad:()=>loadResolve(),internalStop:()=>engine.requestStop('internal failure')};
}
test('bootstrap evaluation and init return without any native installation or bridge load',()=>{
  const h=harness();assert.equal(h.installed,0);assert.equal(h.loaded,0);
  h.api.init('early',base);assert.equal(h.installed,0);assert.equal(h.loaded,0);
  assert.equal(h.events[0].event,'attached');
});
test('uses precisely mock_treat without location-entry observer or coordinate read',()=>{
  const h=harness();h.api.init('early',base);h.flush();
  assert.equal(h.installed,1);assert.equal(h.loaded,0);
  const ready=h.events.find(e=>e.event==='native_location_ready');
  assert.equal(ready.implementation,'rootcatch-profile7-v2');assert.equal(ready.hookCount,3);
});
test('loads farmer only after warm interval and both libraries are ready',async()=>{
  const h=harness();h.api.init('early',base);h.flush();h.setLibraries(false);
  await h.advance(ENGINE_WARM_MS);assert.equal(h.loaded,0);
  h.setLibraries(true);await h.advance(500);assert.equal(h.loaded,1);
  assert.equal(h.params.nativeLocationJNI,false);assert.equal(h.params.automaticPreflight,true);
  assert.equal(h.params.autoCatch,true);assert.equal(h.params.autoSpin,true);
});
test('disabled checkbox installs no JNI hook and keeps existing configurations',async()=>{
  const h=harness();h.api.init('warm',{...base,resolveError12:false,sdk:35,
    transferEnabled:true,keepShiny:true,keepHundo:true,maxTransfers:0,cleanBag:true,discard:{RazzBerry:5}});
  h.flush();await h.advance(500);
  assert.equal(h.installed,0);assert.equal(h.loaded,1);
  assert.equal(h.params.transferEnabled,true);assert.deepEqual(h.params.discard,{RazzBerry:5});
  assert.equal(h.params.maxTransfers,0);assert.equal(h.params.keepShiny,true);assert.equal(h.params.keepHundo,true);
});
test('no diagnostic 300-second stop in the continuous service',async()=>{
  const h=harness();h.api.init('early',base);h.flush();
  for(let i=0;i<600;i++)await h.advance(1000);
  assert.equal(h.loaded,1);assert.equal(h.closed,0);
  assert.equal(h.events.filter(e=>e.event==='stopped').length,0);
});
test('stop before deferred install creates no hooks and starts no engine',async()=>{
  const h=harness();h.api.init('early',base);
  h.api.onFridaStdin(JSON.stringify({type:'stop',sessionId:base.sessionId}));
  h.flush();await h.advance(ENGINE_WARM_MS);
  assert.equal(h.installed,0);assert.equal(h.loaded,0);
  assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,true);
});
test('stop during pending engine load releases hooks and never calls engine init',async()=>{
  const h=harness({pendingLoad:true});h.api.init('early',base);h.flush();await h.advance(ENGINE_WARM_MS);
  h.api.onFridaStdin(JSON.stringify({type:'stop',sessionId:base.sessionId}));
  assert.equal(h.closed,1);h.resolveLoad();await h.advance(500);assert.equal(h.params,undefined);
});
test('controller lease failure cleans location even before IL2CPP starts',async()=>{
  const h=harness();h.api.init('early',base);h.flush();await h.advance(LEASE_MS+1,false);
  assert.equal(h.closed,1);assert.equal(h.loaded,0);
  assert.match(h.events.find(e=>e.event==='stopped').reason,/connection lost/);
  assert.equal(h.events.find(e=>e.event==='control_timeout').stopAfterMs,10000);
});
test('bootstrap keeps the existing location scope during a recoverable controller delay',async()=>{
  const h=harness();h.api.init('early',base);h.flush();h.setLibraries(false);
  await h.advance(4000,false);assert.equal(h.closed,0);assert.equal(h.loaded,0);
  await h.advance(500,true);assert.equal(h.closed,0);
  assert.ok(!h.events.some(e=>e.event==='stopped'));
});
test('internal engine stop releases location only once and ends bootstrap timer',async()=>{
  const h=harness();h.api.init('early',base);h.flush();await h.advance(ENGINE_WARM_MS);
  h.internalStop();await h.advance(10000,false);h.api.dispose();assert.equal(h.closed,1);
});
test('wrong-session commands never reach the motor and trigger controlled stop',async()=>{
  const h=harness();h.api.init('early',base);h.flush();await h.advance(ENGINE_WARM_MS);
  const before=h.forwarded.length;h.beat({sessionId:'other'});
  assert.equal(h.forwarded.length,before);assert.equal(h.closed,1);
});
test('invalid or duplicate initialization cannot install a second hook set',()=>{
  for(const patch of [{sdk:35},{edition:'other'},{nativeLocationJNI:true},{sessionId:'bad'},{readOnly:true},{resolveError12:'yes'}]){
    const h=harness();assert.throws(()=>h.api.init('early',{...base,...patch}));assert.equal(h.installed,0);
  }
  const h=harness();h.api.init('early',base);assert.throws(()=>h.api.init('early',base));h.flush();assert.equal(h.installed,1);
});
test('installation rollback failure cannot be reported as successful cleanup',()=>{
  const h=harness({installError:true,rollback:{passed:false,remainingHooks:1,remainingClassRefs:1,errors:['rollback failed']}});
  h.api.init('early',base);h.flush();assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,false);
});
test('failed engine load still rolls back location hooks',async()=>{
  const h=harness({loadError:true});h.api.init('early',base);h.flush();await h.advance(ENGINE_WARM_MS);
  assert.equal(h.closed,1);assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,true);
});
test('failed engine stop still attempts own-hook cleanup',async()=>{
  const h=harness({stopError:true,cleanupError:true});h.api.init('early',base);h.flush();await h.advance(ENGINE_WARM_MS);
  h.api.dispose();assert.equal(h.closed,1);assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,false);
});
