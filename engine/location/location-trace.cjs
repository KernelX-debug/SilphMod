'use strict';
const {jniRuntime} = require('./jni-runtime.cjs');
const {resolveNativeEntry,sampleGate,PROFILES} = require('./trace-policy.cjs');
const SIGNATURE = '(Ljava/lang/String;Landroid/location/Location;ILandroid/content/Context;)V';
const GETTERS = Object.freeze([
  ['latitude','getLatitude','()D','double'], ['longitude','getLongitude','()D','double'],
  ['accuracyMeters','getAccuracy','()F','float'], ['speedMps','getSpeed','()F','float'],
  ['altitudeMeters','getAltitude','()D','double'], ['timeMs','getTime','()J','long'],
  ['elapsedRealtimeNanos','getElapsedRealtimeNanos','()J','long'],
  ['rawMockFlag','isFromMockProvider','()Z','boolean'],
]);

function installTrace(runtime,parameters,report) {
  const features=PROFILES[parameters.profile];
  if(!features?.jni)throw Error('This profile cannot prepare JNI');
  if(!Number.isInteger(parameters.sdk)||parameters.sdk<29)throw Error('Android 10 or newer required');
  const {Process,Interceptor,ptr} = runtime;
  let active=false, configured=false, jni=null, nativeObserved=false, actualSdk=null;
  let queries=0, changed=0, updates=0, returned=0, samples=0, lastSummary=0, skippedOwnQueries=0;
  const listeners=[], globals=[], mockIds=new Map(), getters=[], ownReads=new Set(), statuses={}, statusCounts={};
  const gate=sampleGate(()=>Date.now());
  const treatment=features.treatment;
  function snapshot() {
    return {sdk:actualSdk,mockQueries:queries,mockResultsChanged:changed,locationUpdates:updates,
      nativeReturns:returned,samples,skippedOwnQueries,statusCounts:{...statusCounts},
      nativeEntryObserved:nativeObserved,nativeCalls:jni?.nativeCalls??0,
      hooksInstalled:listeners.length,coordinateMutation:false,locationObjectMutation:false,
      systemMutation:false,serverLocationAcceptanceObserved:false,features:{...features},backtraceEnabled:parameters.backtrace===true,
      ownJniTrapsSuppressed:true};
  }
  function caller(context) {
    const address=context.returnAddress;
    let module=null;
    try { module=address?Process.findModuleByAddress(address):null; } catch(_) {}
    return {threadId:Process.getCurrentThreadId(),returnAddress:address?.toString()??null,
      module:module?.name??null,offset:module&&address?address.sub(module.base).toString():null};
  }
  function backtrace(context) {
    if(parameters.backtrace!==true)return {disabled:true};
    try {
      return {kind:'accurate',frames:runtime.Thread.backtrace(context.context,runtime.Backtracer.ACCURATE)
        .slice(0,4).map(p=>runtime.DebugSymbol.fromAddress(p).toString().slice(0,180))};
    } catch(error) { return {unavailable:String(error)}; }
  }
  function close() {
    active=false; const errors=[];
    for (const listener of listeners.splice(0)) {
      try { listener.detach(); } catch(error) { errors.push(String(error)); listeners.push(listener); }
    }
    // Retain classes while any callback might still be installed.
    if (jni && !listeners.length && globals.length) {
      try { jni.withEnv(scope=>{
        for (const ref of globals.splice(0)) {
          try { scope.release(ref); } catch(error) { globals.push(ref);errors.push(String(error)); }
        }
      }); } catch(error) { errors.push(String(error)); }
    }
    return {passed:errors.length===0,remainingHooks:listeners.length,remainingClassRefs:globals.length,errors,...snapshot()};
  }
  function summary(now,force=false) {
    if (force || active && now-lastSummary>=2500) { lastSummary=now;report('trace_summary',snapshot()); }
  }
  try {
    report('jni_bootstrap_started',{treatment});
    jni=jniRuntime(runtime,report);
    jni.withEnv(scope=>{
      const sdk=scope.sdk();
      if(sdk!==parameters.sdk) throw Error('Process and controller SDK mismatch');
      actualSdk=sdk;
      if(features.mock||features.read||parameters.profile==='jni_prepare') {
        const location=scope.findClass('android/location/Location');
        globals.push(scope.retain(location));
        for(const name of ['isFromMockProvider','isMock']) {
          try { const id=scope.method(location,name,'()Z');mockIds.set(id.toString(),name); }
          catch(error) { report('mock_method_unavailable',{method:name,message:String(error)}); }
        }
        if(![...mockIds.values()].includes('isFromMockProvider')) throw Error('Required mock method missing');
        if(sdk>=31&&![...mockIds.values()].includes('isMock'))throw Error('Required modern mock method missing');
        if(features.read)for(const [key,name,signature,type] of GETTERS) {
          try { getters.push({key,id:scope.method(location,name,signature),type}); }
          catch(error) { report('location_getter_unavailable',{key,message:String(error)}); }
        }
        if(features.read)scope.prepareLocationRead();
      }
      let nativeEntry=null;
      if(features.entry)try {
        if(sdk!==29)throw Error('ART entry observation limited to SDK 29; offsets are not guessed');
        const manager=scope.loadApplicationClass('com.nianticlabs.nia.location.NianticLocationManager');
        globals.push(scope.retain(manager));
        const method=scope.method(manager,'nativeLocationUpdate',SIGNATURE);
        const module=Process.findModuleByName('libNianticLabsPlugin.so');
        const resolved=resolveNativeEntry(runtime,jni,method,module,sdk);
        nativeEntry=resolved.entry;
        report('native_entry_validated',{method:'NianticLocationManager.nativeLocationUpdate',signature:SIGNATURE,
          address:nativeEntry.toString(),library:module.name,relativeOffset:nativeEntry.sub(module.base).toString(),
          sdk,artDataOffset:resolved.offset,artAccessFlags:resolved.flags,layoutIsNotPublicABI:true});
        try { Object.assign(statuses,scope.enumNames(scope.loadApplicationClass('com.nianticlabs.nia.contextservice.ServiceStatus'))); }
        catch(error) { report('service_status_names_unavailable',{message:String(error),ordinalsStillRecorded:true}); }
        report('service_status_names',{names:statuses});
      } catch(error) { report('native_entry_unavailable',{message:String(error),noGuessing:true});throw error; }
      const targets=new Map();
      if(features.mock)for(const [index,name] of [[37,'CallBooleanMethod'],[38,'CallBooleanMethodV'],[39,'CallBooleanMethodA']]) {
        const address=jni.slot(scope.table,index), key=address.toString();
        if(!targets.has(key))targets.set(key,{address,variants:[]});
        targets.get(key).variants.push(name);
      }
      report('trace_targets_resolved',{mockMethods:[...mockIds.values()],
        jniTargets:[...targets.values()].map(t=>({address:t.address.toString(),variants:t.variants})),nativeEntryAvailable:!!nativeEntry});
      for(const target of targets.values())listeners.push(Interceptor.attach(target.address,{
        onEnter(args) {
          this.rcMethod=active?mockIds.get(args[2].toString()):null;
          this.rcOwnRead=ownReads.has(Process.getCurrentThreadId());
        },
        onLeave(result) {
          if(!active||!this.rcMethod)return;
          if(this.rcOwnRead){skippedOwnQueries++;return;}
          queries++;const before=result.toUInt32()&0xff;
          if(treatment){result.replace(ptr(0));if(before!==0)changed++;}
          if(gate('mock:'+this.rcMethod)) {
            samples++;
            report('mock_query',{method:this.rcMethod,before,after:treatment?0:before,
              responseChanged:treatment&&before!==0,caller:caller(this),backtrace:backtrace(this),...snapshot()});
          }
        },
      }));
      if(nativeEntry) {
        listeners.push(Interceptor.attach(nativeEntry,{
          onEnter(args) {
            this.rcTraceActive=active;
            if(!active)return;
            updates++;const ordinal=args[4].toInt32();
            const key=Object.keys(statusCounts).length<64||String(ordinal)in statusCounts?String(ordinal):'other';
            statusCounts[key]=(statusCounts[key]??0)+1;
            const statusName=statuses[ordinal]??null;
            if(!gate('location:'+key))return;
            this.rcSample=true;this.rcEntered=Date.now();
            const detail={sequence:updates,statusOrdinal:ordinal,statusName,locationIsNull:args[3].isNull(),
              caller:caller(this),backtrace:backtrace(this),values:{},sampleErrors:[],
              statusIsProviderStateNotServerAcceptance:true,samplingReadDisabled:!features.read};
            if(!features.read){samples++;report('location_update',{...detail,...snapshot()});return;}
            const tid=Process.getCurrentThreadId();
            if(ownReads.has(tid)){detail.sampleErrors.push('nested_own_read');}
            else {
              ownReads.add(tid);let sampleScope;
              try {
                sampleScope=jni.fromEnv(args[0]);
                if(sampleScope.pending())detail.sampleErrors.push('preexisting_jni_exception_preserved');
                else {
                  try {detail.provider=sampleScope.text(args[2]);} catch(error) {detail.sampleErrors.push(String(error));}
                  if(!args[3].isNull())for(const getter of getters) {
                    try {
                      const value=sampleScope.scalar(args[3],getter.id,getter.type);
                      detail.values[getter.key]=getter.type==='long'?value.toString():getter.type==='boolean'?value!==0:value;
                    } catch(error) {detail.sampleErrors.push(getter.key+':'+String(error));}
                  }
                }
              } catch(error) {detail.sampleErrors.push(String(error));}
              finally {
                if(sampleScope)detail.sampleErrors.push(...sampleScope.close());
                ownReads.delete(tid);
              }
            }
            samples++;report('location_update',{...detail,...snapshot()});
          },
          onLeave() {
            if(!active||!this.rcTraceActive)return;
            returned++;
            if(this.rcSample)report('location_dispatch_return',{durationMs:Date.now()-this.rcEntered,
              nativeVoidReturnIsNotAcceptance:true,...snapshot()});
          },
        }));
        nativeObserved=true;
      }
    });
    active=true;configured=true;
    report('trace_hooks_ready',{...snapshot(),jniBooleanTreatment:treatment,
      coreTreatmentAvailable:true,observerIncludesGameRPC:false});
    return {close,summary,snapshot,get configured(){return configured;}};
  } catch(error) {
    const rollback=close();report('trace_setup_failed',{message:String(error),rollback});
    const failed=new Error(String(error));failed.traceRollback=rollback;throw failed;
  }
}
module.exports={installTrace,SIGNATURE,GETTERS};
