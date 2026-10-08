// Actual timer and input handler with a fake clock/native boundary. No captured code is executed.
const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),esbuild=require('esbuild');
const compiled=esbuild.build({entryPoints:['src/agent.ts'],bundle:true,write:false,format:'iife',platform:'neutral',
  plugins:[{name:'control-boundary',setup(build){
    build.onResolve({filter:/^frida-(il2cpp|java)-bridge$/},a=>({path:a.path,namespace:'mock'}));
    build.onLoad({filter:/.*/,namespace:'mock'},a=>({contents:a.path==='frida-java-bridge'?'export default {};':'globalThis.Il2Cpp={};',loader:'js'}));
    build.onLoad({filter:/[\\/]agent\.ts$/},a=>({loader:'ts',contents:fs.readFileSync(a.path,'utf8')+`
      globalThis.__watchdogTest={
        seed(recovery){initialized=true;config={controlRecovery:recovery,sessionAccounting:false,diagnosticTrace:false};
          lastBeat=Date.now();locationAdapter=globalThis.__location;gc=globalThis.__gc;},
        beat(command){enginePort.onFridaStdin(JSON.stringify({type:'heartbeat',foreground:true,...command}));},
        userStop(){stop('user');},state(){return {stopped,allowed,lastBeat,lastHeartbeatSequence};}
      };` }));
  }}]}).then(r=>r.outputFiles[0].text);
async function harness(recovery=true) {
  let now=10000,closed=0,gcClosed=0;const events=[],timers=[];
  const context={Date:{now:()=>now},rpc:{exports:{}},setInterval:fn=>{timers.push(fn);return 1;},clearInterval:()=>{},
    setImmediate:()=>{},Process:{getCurrentThreadId:()=>1},
    send:message=>events.push(JSON.parse(message[1]).payload),
    __location:{close:()=>{closed++;return {passed:true,remainingHooks:0,remainingClassRefs:0,errors:[]};},summary:()=>{}},
    __gc:{count:0,close:()=>{gcClosed++;return {passed:true,remaining:0,errors:[]};}}};
  vm.runInNewContext(await compiled,context);const api=context.__watchdogTest;api.seed(recovery);
  return {api,events,advance:ms=>{now+=ms;for(const timer of timers)timer();},
    get closed(){return closed;},get gcClosed(){return gcClosed;}};
}
test('actual Silph timer tolerates a four-second delay without closing location or sending game requests',async()=>{
  const h=await harness();h.advance(4000);
  assert.equal(h.api.state().stopped,false);assert.equal(h.closed,0);assert.equal(h.gcClosed,0);
  const delayed=h.events.find(e=>e.event==='control_delay');assert.equal(delayed.heartbeatAgeMs,4000);
  assert.equal(delayed.newActionsAllowed,false);assert.equal(delayed.connectionStopped,false);
  assert.equal(delayed.locationTreatmentRetained,true);assert.equal(delayed.stopAfterMs,10000);
  assert.ok(!h.events.some(e=>e.event==='request'||e.event==='stopped'||e.event==='native_location_cleanup'));
});
test('fresh sequenced control recovers the existing connection without reinstalling hooks',async()=>{
  const h=await harness();h.advance(4000);h.api.beat({sequence:1,sentAtElapsedMs:4444});h.advance(500);
  assert.equal(h.api.state().stopped,false);assert.equal(h.closed,0);
  const recovered=h.events.find(e=>e.event==='control_recovered');assert.equal(recovered.lastHeartbeatSequence,1);
  assert.equal(recovered.lastHeartbeatSentAt,4444);assert.equal(recovered.newActionsStillRequireFocusAndSessionValidation,true);
});
test('real controller loss closes owned state once and cannot be revived by a later heartbeat',async()=>{
  const h=await harness();h.advance(10001);
  assert.equal(h.api.state().stopped,true);assert.equal(h.closed,1);assert.equal(h.gcClosed,1);
  assert.equal(h.events.find(e=>e.event==='control_timeout').heartbeatAgeMs,10001);
  h.api.beat({sequence:1,sentAtElapsedMs:4444});h.advance(1000);h.api.userStop();assert.equal(h.closed,1);
});
test('legacy builds retain their prior three-second deadman until explicitly opted in',async()=>{
  const h=await harness(false);h.advance(3001);assert.equal(h.api.state().stopped,true);assert.equal(h.closed,1);
  assert.equal(h.events.find(e=>e.event==='stopped').reason,'Engine control connection lost');
});
test('Stop during a temporary delay is immediate and still releases location',async()=>{
  const h=await harness();h.advance(4000);h.api.userStop();assert.equal(h.closed,1);assert.equal(h.api.state().stopped,true);
});
test('missing or repeated control sequences fail closed rather than renewing a replayed lease',async()=>{
  for(const commands of [[{}],[{sequence:1,sentAtElapsedMs:1000},{sequence:1,sentAtElapsedMs:1500}]]) {
    const h=await harness();for(const command of commands)h.api.beat(command);
    assert.equal(h.api.state().stopped,true);assert.equal(h.closed,1);
    assert.ok(h.events.some(e=>e.event==='error'));
  }
});
