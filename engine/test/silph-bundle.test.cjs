const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),esbuild=require('esbuild');
const compiled=esbuild.build({entryPoints:['src/silph-entry.ts'],bundle:true,write:false,format:'iife',platform:'neutral',
  plugins:[{name:'native-fixtures',setup(build){
    build.onResolve({filter:/^frida-(il2cpp|java)-bridge$/},args=>({path:args.path,namespace:'mock'}));
    build.onLoad({filter:/.*/,namespace:'mock'},args=>({contents:args.path==='frida-java-bridge'?
      'globalThis.__bridgeLoads++;export default globalThis.__mockJava;':
      'globalThis.__bridgeLoads++;globalThis.Il2Cpp=globalThis.__mockIl2Cpp;',loader:'js'}));
    build.onLoad({filter:/location-trace\.cjs$/},()=>({contents:'module.exports=globalThis.__mockTrace;',loader:'js'}));
  }}]}).then(result=>result.outputFiles[0].text);
async function harness(enabled=true) {
  let now=10000,installed=0,closed=0;const events=[],deferred=[],intervals=[];
  const context={rpc:{exports:{}},__bridgeLoads:0,__mockJava:{perform:()=>{}},__mockIl2Cpp:{},
    Process:{id:65160,arch:'arm64',pointerSize:8,findModuleByName:()=>({})},Module:{},Memory:{},
    NativeFunction:function(){throw Error('Unexpected native call');},Interceptor:{},ptr:()=>{},
    setImmediate:fn=>deferred.push(fn),setInterval:fn=>{intervals.push(fn);return fn;},clearInterval:fn=>{
      const i=intervals.indexOf(fn);if(i>=0)intervals.splice(i,1);},
    Date:class extends Date {static now(){return now;}},
    send:message=>events.push(JSON.parse(message[1]).payload),
    __mockTrace:{installTrace:(runtime,p,report)=>{installed++;report('trace_targets_resolved',{mockMethods:['isFromMockProvider']});
      return {snapshot:()=>({hooksInstalled:3,coordinateMutation:false,systemMutation:false}),summary:()=>{},
        close:()=>{closed++;return {passed:true,remainingHooks:0,remainingClassRefs:0,errors:[]};}};}},
  };
  vm.runInNewContext(await compiled,context);
  const api=context.rpc.exports;
  const params={edition:'ugphone',automaticPreflight:true,readOnly:false,encounterOnly:false,
    gamePackage:'com.nianticlabs.pokemongo',version:'0.431.0',sessionId:'1791382837459_23162511',
    resolveError12:enabled,nativeLocationJNI:false,sdk:29,autoCatch:true,autoSpin:true,cleanBag:false};
  const command=type=>api.onFridaStdin(JSON.stringify({type,sessionId:params.sessionId,foreground:true}));
  return {api,params,events,context,command,get installed(){return installed;},get closed(){return closed;},
    flush(){while(deferred.length)deferred.shift()();},async advance(ms){now+=ms;command('heartbeat');
      for(const fn of [...intervals])fn();await new Promise(resolve=>setImmediate(resolve));}};
}
test('actual bundled entry defers both bridges and original engine until after location setup',async()=>{
  const h=await harness();assert.equal(h.context.__bridgeLoads,0);
  h.api.init('early',h.params);assert.equal(h.installed,0);assert.equal(h.context.__bridgeLoads,0);
  h.flush();assert.equal(h.installed,1);assert.equal(h.context.__bridgeLoads,0);
  await h.advance(7000);assert.equal(h.context.__bridgeLoads,0);
  await h.advance(1000);assert.equal(h.context.__bridgeLoads,2);
  assert.equal(h.context.rpc.exports,h.api);
  assert.equal(h.events.filter(e=>e.event==='native_location_ready').length,1);
  h.command('stop');assert.equal(h.closed,1);
  assert.equal(h.events.find(e=>e.event==='native_location_cleanup').passed,true);
  assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,true);
});
test('actual bundled entry stopped before setup never loads Java or IL2CPP',async()=>{
  const h=await harness();h.api.init('early',h.params);h.command('stop');h.flush();await h.advance(10000);
  assert.equal(h.installed,0);assert.equal(h.context.__bridgeLoads,0);
});
test('unchecked bundled option starts the legacy engine without JNI treatment',async()=>{
  const h=await harness(false);h.api.init('warm',h.params);h.flush();await h.advance(500);
  assert.equal(h.installed,0);assert.equal(h.context.__bridgeLoads,2);
  h.command('stop');assert.equal(h.closed,0);
  assert.equal(h.events.find(e=>e.event==='stopped').cleanupPassed,true);
});
