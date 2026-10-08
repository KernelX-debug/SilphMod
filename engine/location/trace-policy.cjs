'use strict';

function resolveNativeEntry(runtime, jni, method, module, sdk) {
  // Android 10 ARM64 ArtMethod: 20-byte fixed prefix, aligned to 8 => data_ at 24.
  // This is not a public ABI. Never scan neighboring words or guess for other SDKs.
  if (sdk !== 29 || runtime.Process.pointerSize !== 8 || runtime.Process.arch !== 'arm64')
    throw Error('Supported ART observation: SDK 29 ARM64');
  if (!module || module.name !== 'libNianticLabsPlugin.so') throw Error('Location library unavailable');
  if (!method || method.isNull() || !method.and(runtime.ptr(7)).isNull()) throw Error('jmethodID is not an aligned ART pointer');
  jni.readable(method,32);
  const flags = method.add(4).readU32();
  if ((flags & 0x100) === 0 || (flags & 0x8) !== 0 || (flags & 0x2) === 0)
    throw Error('ArtMethod does not match a private native instance method');
  const entry = method.add(24).readPointer();
  if (entry.compare(module.base) < 0 || entry.compare(module.base.add(module.size)) >= 0)
    throw Error('JNI entry outside expected library; not instrumented');
  jni.executable(entry);
  if (!entry.equals(method.add(24).readPointer())) throw Error('JNI entry changed during verification');
  return {entry,flags,offset:24};
}

function sampleGate(clock, first=3, interval=1500) {
  const records = new Map();
  return key => {
    const now = clock(), record = records.get(key) || {emitted:0,last:0};
    if (record.emitted >= first && now-record.last < interval) return false;
    record.emitted++; record.last=now; records.set(key,record); return true;
  };
}
const PROFILES=Object.freeze({
  minimal:{jni:false,mock:false,entry:false,read:false,treatment:false},
  jni_prepare:{jni:true,mock:false,entry:false,read:false,treatment:false},
  mock_observe:{jni:true,mock:true,entry:false,read:false,treatment:false},
  location_count:{jni:true,mock:false,entry:true,read:false,treatment:false},
  location_read:{jni:true,mock:false,entry:true,read:true,treatment:false},
  mock_treat:{jni:true,mock:true,entry:false,read:false,treatment:true},
  combined:{jni:true,mock:true,entry:true,read:true,treatment:true},
});
function validateSession(parameters) {
  if (!parameters || parameters.protocol !== 2 || typeof parameters.session!=='string' || !/^[0-9]{13}_[a-f0-9]{8}$/.test(parameters.session)
      || !Object.prototype.hasOwnProperty.call(PROFILES,parameters.profile) || parameters.extended !== false
      || typeof parameters.backtrace!=='boolean' || parameters.backtrace&&!PROFILES[parameters.profile].mock&&!PROFILES[parameters.profile].entry
      || !Number.isInteger(parameters.sdk) || parameters.sdk < 29 || parameters.sdk > 100)
    throw Error('Invalid diagnostic parameters');
  return {...parameters,features:{...PROFILES[parameters.profile]}};
}
module.exports = {resolveNativeEntry,sampleGate,validateSession,PROFILES};
