'use strict';

// Independent implementation against the public JNI ABI. This module does not
// import/evaluate any recovered agent, change coordinates, or patch Java classes.
const METHODS = Object.freeze(['isFromMockProvider', 'isMock']);
const VARIANTS = Object.freeze([[37, 'CallBooleanMethod'], [38, 'CallBooleanMethodV'], [39, 'CallBooleanMethodA']]);

function installNativeLocation(runtime, report) {
  const {Process, Module, Memory, NativeFunction, Interceptor, ptr} = runtime;
  if (Process.arch !== 'arm64' || Process.pointerSize !== 8)
    throw Error('JNI treatment requires an ARM64 process');
  const size = Process.pointerSize, listeners = [], resolved = new Map();
  let active = false, classRef = null, env = null, vm = null, detachThread = null, deleteLocal = null;
  let summaryAt = 0, calls = 0, changed = 0, emitted = 0;
  const counters = Object.fromEntries(METHODS.map(name => [name, {calls:0, changed:0}]));
  const options = {scheduling:'exclusive', exceptions:'propagate'};
  function readable(address, bytes) {
    if (!address || address.isNull()) throw Error('Null JNI pointer');
    const range = Process.findRangeByAddress(address);
    if (!range || !range.protection.includes('r') || address.add(bytes).compare(range.base.add(range.size)) > 0)
      throw Error('JNI table outside readable memory');
    return address;
  }
  function executable(address) {
    const range = address && !address.isNull() ? Process.findRangeByAddress(address) : null;
    if (!range || !range.protection.includes('x')) throw Error('JNI entry not executable');
    return address;
  }
  function slot(table, index) {
    return executable(readable(table.add(index*size), size).readPointer());
  }
  function bind(table, index, result, arguments_) {
    return new NativeFunction(slot(table,index),result,arguments_,options);
  }
  function snapshot() {
    return {calls, changed, methodCounts:Object.fromEntries(METHODS.map(name => [name,{...counters[name]}])),
      coordinateMutation:false, systemMutation:false};
  }
  function close() {
    active = false;
    const errors = [];
    for (const listener of listeners.splice(0)) {
      try {listener.detach();} catch(error) {errors.push(String(error));listeners.push(listener);}
    }
    return {passed:errors.length===0, remaining:listeners.length, errors, ...snapshot()};
  }
  try {
    const art = Process.findModuleByName('libart.so');
    const export_ = art?.findExportByName('JNI_GetCreatedJavaVMs') ?? Module.findGlobalExportByName('JNI_GetCreatedJavaVMs');
    if (!export_) throw Error('JNI_GetCreatedJavaVMs unavailable');
    executable(export_);
    const getVMs = new NativeFunction(export_, 'int', ['pointer','int','pointer'],options);
    const vmOut = Memory.alloc(size), countOut = Memory.alloc(4);
    vmOut.writePointer(ptr(0)); countOut.writeS32(0);
    const status = getVMs(vmOut,1,countOut);
    if (status !== 0 || countOut.readS32() !== 1) throw Error('No unique JVM found');
    vm = readable(vmOut.readPointer(),size);
    const vmTable = readable(vm.readPointer(),size);
    const getEnv = bind(vmTable,6,'int',['pointer','pointer','int']);
    const envOut = Memory.alloc(size); envOut.writePointer(ptr(0));
    const envStatus = getEnv(vm,envOut,0x00010006);
    if (envStatus === -2) {
      const attach = bind(vmTable,7,'int',['pointer','pointer','pointer']); // AttachCurrentThreadAsDaemon
      const detach = bind(vmTable,5,'int',['pointer']);
      if (attach(vm,envOut,ptr(0)) !== 0) throw Error('Could not attach owned thread to JVM');
      detachThread = detach;
    } else if (envStatus !== 0) throw Error('GetEnv rejected JNI 1.6: '+envStatus);
    env = readable(envOut.readPointer(),size);
    const table = readable(env.readPointer(),size);
    const hasException = bind(table,228,'uint8',['pointer']); // ExceptionCheck: jboolean
    const clearException = bind(table,17,'void',['pointer']);
    deleteLocal = bind(table,23,'void',['pointer','pointer']);
    if (hasException(env)) throw Error('Thread already has a JNI exception; unchanged');
    const findClass = bind(table,6,'pointer',['pointer','pointer']);
    const getMethod = bind(table,33,'pointer',['pointer','pointer','pointer','pointer']);
    classRef = findClass(env,Memory.allocUtf8String('android/location/Location'));
    if (hasException(env)) {
      clearException(env); throw Error('FindClass(Location) raised an owned exception');
    }
    if (classRef.isNull()) throw Error('Location class unavailable');
    const signature = Memory.allocUtf8String('()Z'), missing = [];
    for (const name of METHODS) {
      const id = getMethod(env,classRef,Memory.allocUtf8String(name),signature);
      if (hasException(env)) {clearException(env); missing.push(name); continue;}
      if (id.isNull()) {missing.push(name); continue;}
      const key = id.toString();
      if (resolved.has(key)) throw Error('Ambiguous JNI IDs');
      resolved.set(key,name);
    }
    if (![...resolved.values()].includes('isFromMockProvider'))
      throw Error('Could not resolve the required location method');
    // Resolve and validate all targets before touching code. Different JNI
    // variants may share an entry point; attach each distinct address once.
    const targets = new Map();
    for (const [index,variant] of VARIANTS) {
      const address = slot(table,index), key = address.toString();
      if (!targets.has(key)) targets.set(key,{address,variants:[]});
      targets.get(key).variants.push(variant);
    }
    for (const {address} of targets.values()) listeners.push(Interceptor.attach(address,{
      onEnter(args) {
        this.rootcatchLocation = active ? resolved.get(args[2].toString()) : undefined;
      },
      onLeave(result) {
        const name = this.rootcatchLocation;
        if (!active || !name) return;
        const before = result.toUInt32() & 0xff;
        calls++; counters[name].calls++;
        result.replace(ptr(0));
        if (before !== 0) {changed++; counters[name].changed++;}
        // Never log other JNI calls, objects, coordinates or per-call traffic.
        if (emitted < 6) {
          emitted++;
          report('native_location_sample',{method:name,before,after:0,calls,changed});
        }
      },
    }));
    active = true;
    report('native_location_ready',{implementation:'rootcatch-jni-v1',
      methods:[...resolved.values()],missing, hookCount:listeners.length,
      variants:[...targets.values()].flatMap(target => target.variants), ...snapshot()});
    return {
      close,
      summary(now) {
        if (active && now-summaryAt >= 30000) {
          summaryAt = now; report('native_location_summary',snapshot());
        }
      },
    };
  } catch(error) {
    const rollback = close();
    report('native_location_failed',{message:String(error),rollback});
    throw error;
  } finally {
    let releaseError = null;
    try {if (deleteLocal && classRef && !classRef.isNull()) deleteLocal(env,classRef);}
    catch(error) {releaseError=error;}
    // A JNIEnv belongs to its thread. No env or local jobject is retained by
    // callbacks; only method IDs survive. Never detach someone else's thread.
    if (detachThread) {
      try {
        const status = detachThread(vm);
        if (status !== 0) releaseError=Error('Could not release owned JNI thread: '+status);
      } catch(error) {releaseError=error;}
    }
    if(releaseError){
      const rollback=close();
      report('native_location_failed',{message:String(releaseError),rollback});
      throw releaseError;
    }
  }
}

module.exports = {installNativeLocation, METHODS, VARIANTS};
