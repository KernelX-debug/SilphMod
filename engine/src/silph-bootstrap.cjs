'use strict';

// No bridge, Java/IL2CPP lookup or native call at module evaluation time.
// Profile 7 runs first; the existing farmer is loaded only afterwards.
const IMPLEMENTATION = 'rootcatch-profile7-v2';
const {SILPH_STOP_MS,ACTION_LEASE_MS}=require('./control-watchdog.cjs');
const LEASE_MS = SILPH_STOP_MS, ENGINE_WARM_MS = 8000;
function createBootstrap(runtime, install, loadEngine, emit, clock, timers) {
  let parameters=null, adapter=null, engine=null, phase='idle', stopped=false;
  let lastBeat=0, locationAt=0, librariesAt=null, engineLoading=false, methods=[];
  let latestHeartbeat=null, cleanup=null;
  function release() {
    if(cleanup)return cleanup;
    try { cleanup=adapter?adapter.close():{passed:true,remainingHooks:0,remainingClassRefs:0,errors:[]}; }
    catch(error) {cleanup={passed:false,remainingHooks:null,remainingClassRefs:null,errors:[String(error)]};}
    adapter=null;return cleanup;
  }
  function locationReport(event,detail) {
    if(event==='trace_targets_resolved')methods=detail.mockMethods??[];
    emit('location_trace',{traceEvent:event,...detail});
  }
  function error(detail) {
    emit('error',{stage:'silph_bootstrap:'+phase,message:String(detail)});
    stop('SilphMod startup: '+String(detail));
  }
  function stop(reason) {
    if(stopped)return;
    stopped=true;phase='stopped';timers.clearInterval(timer);
    if(engine) {
      try {engine.requestStop(reason);return;}catch(failed){emit('error',{stage:'silph_bootstrap:stop',message:String(failed)});}
    }
    const result=release();
    if(parameters?.resolveError12)emit('native_location_cleanup',result);
    emit('gc_cleanup',{passed:result.passed,remaining:0,errors:result.errors??[],handleBits:64});
    emit('stopped',{reason,cleanupPassed:result.passed,caught:0,fled:0,spun:0,discarded:0,transferred:0});
  }
  async function startEngine() {
    if(stopped||engineLoading)return;
    engineLoading=true;phase='engine_loading';emit('bootstrap_stage',{phase});
    try {
      const loaded=await loadEngine();
      if(stopped)return;
      engine=loaded;
      if(!engine||typeof engine.init!=='function'||typeof engine.bindLocationAdapter!=='function'
          ||typeof engine.observeEvents!=='function'||typeof engine.requestStop!=='function')throw Error('Invalid engine contract');
      engine.observeEvents(event=>{
        if(event==='stopped'){stopped=true;phase='stopped';timers.clearInterval(timer);}
      });
      if(adapter)engine.bindLocationAdapter({
        close:release,
        summary:now=>adapter?.summary(now),
      });
      // The legacy JNI implementation is explicitly disabled. Only the
      // already installed/validated profile 7 owns location hooks here.
      engine.init('silph-deferred',{...parameters,nativeLocationJNI:false});
      if(latestHeartbeat)engine.onFridaStdin(JSON.stringify(latestHeartbeat));
      if(!stopped){phase='engine_started';emit('bootstrap_stage',{phase,singleAttach:true,resolveError12:parameters.resolveError12});}
    } catch(failed) {if(!stopped)error(failed);}
  }
  const timer=timers.setInterval(()=>{
    if(!parameters||stopped)return;
    if(clock()-lastBeat>LEASE_MS){
      emit('control_timeout',{stage:'bootstrap',heartbeatAgeMs:clock()-lastBeat,lastBeatAt:lastBeat,
        lastHeartbeatSequence:latestHeartbeat?.sequence??0,actionLeaseMs:ACTION_LEASE_MS,
        stopAfterMs:LEASE_MS,connectionStopped:true});
      stop('SilphMod connection lost for over 10 seconds');return;
    }
    if(adapter)adapter.summary(clock());
    if(phase!=='waiting_libraries')return;
    if(!runtime.librariesReady()){librariesAt=null;return;}
    if(parameters.sdk>=30) {
      if(librariesAt===null){librariesAt=clock();emit('bootstrap_stage',{phase:'waiting_runtime',warmMs:ENGINE_WARM_MS});}
      if(clock()-librariesAt<ENGINE_WARM_MS)return;
    }else if(clock()-locationAt<(parameters.resolveError12?ENGINE_WARM_MS:0))return;
    void startEngine();
  },500);
  return {
    init(_stage,p) {
      if(parameters||stopped)throw Error('Session duplicated or stopped');
      if(!p||p.edition!=='ugphone'||p.automaticPreflight!==true||p.readOnly!==false||p.encounterOnly!==false
          ||p.gamePackage!=='com.nianticlabs.pokemongo'||p.version!=='0.433.0'
          ||typeof p.sessionId!=='string'||!/^[0-9]{13}_[a-f0-9]{8}$/.test(p.sessionId)
          ||typeof p.resolveError12!=='boolean'||p.nativeLocationJNI!==false
          ||runtime.Process.arch!=='arm64'||runtime.Process.pointerSize!==8
          ||!Number.isInteger(p.sdk)||p.sdk<29||p.mode!==undefined&&p.mode!=='FARMER')throw Error('Unsupported SilphMod profile');
      parameters={...p};lastBeat=clock();phase='location_setup';
      emit('attached',{pid:runtime.Process.id,arch:runtime.Process.arch,readOnly:true,bootstrap:true});
      emit('silph_bootstrap',{implementation:IMPLEMENTATION,resolveError12:p.resolveError12,
        singleAttach:true,engineDeferred:true,leaseMs:LEASE_MS,engineWarmMs:p.sdk>=30||p.resolveError12?ENGINE_WARM_MS:0});
      timers.setImmediate(()=>{
        if(stopped)return;
        try {
          if(p.resolveError12) {
            adapter=install(runtime,{protocol:2,session:p.sessionId,profile:'mock_treat',
              extended:false,backtrace:false,sdk:p.sdk},locationReport);
            const snapshot=adapter.snapshot();
            if(!methods.includes('isFromMockProvider')||p.sdk>=31&&!methods.includes('isMock')||snapshot.hooksInstalled<1||snapshot.hooksInstalled>3
                ||snapshot.coordinateMutation!==false||snapshot.systemMutation!==false)throw Error('Location not validated');
            emit('native_location_ready',{implementation:IMPLEMENTATION,methods,
              hookCount:snapshot.hooksInstalled,...snapshot});
          }else emit('native_location_disabled',{enabled:false});
          locationAt=clock();phase='waiting_libraries';emit('bootstrap_stage',{phase});
        }catch(failed){if(failed.traceRollback)cleanup=failed.traceRollback;error(failed);}
      });
    },
    onFridaStdin(line) {
      if(stopped||!parameters)return;
      try {
        if(typeof line!=='string'||line.length>4096)throw Error('Control message too long');
        const command=JSON.parse(line);
        if(command.sessionId!==parameters.sessionId)throw Error('Control belongs to another session');
        if(command.type==='stop'){stop(typeof command.reason==='string'?command.reason.slice(0,512):'Stopped by user');return;}
        if(command.type==='heartbeat'){
          if(typeof command.foreground!=='boolean')throw Error('Invalid focus');
          lastBeat=clock();latestHeartbeat=command;
        }else if(command.type!=='activate')throw Error('Command not allowed');
        if(engine)engine.onFridaStdin(line);
      }catch(failed){error(failed);}
    },
    dispose(){stop('Agente retirado');},
    getFridaTerminalMode(){return 'cooked';},
  };
}
module.exports={createBootstrap,IMPLEMENTATION,LEASE_MS,ENGINE_WARM_MS};
