'use strict';

// Public JNI ABI only. No JNIEnv or local jobject is shared across threads.
// Our diagnostic JNI calls must not recursively invoke our own interceptors.
const OPTIONS = Object.freeze({scheduling:'cooperative', exceptions:'propagate', traps:'none'});
function jniRuntime(r, report) {
  const {Process, Module, Memory, NativeFunction, ptr} = r;
  if (Process.arch !== 'arm64' || Process.pointerSize !== 8) throw Error('ARM64 required');
  let nativeCalls = 0;const functionCache=new Map();
  function readable(address, bytes) {
    if (!address || address.isNull()) throw Error('Null pointer');
    const inspected = Process.arch==='arm64'&&Process.pointerSize===8 ? address.and(ptr('0x00ffffffffffffff')) : address;
    const range = Process.findRangeByAddress(inspected);
    if (!range || !range.protection.includes('r') || inspected.add(bytes).compare(range.base.add(range.size)) > 0)
      throw Error('Range not readable: '+address);
    return address;
  }
  function executable(address) {
    const range = address && !address.isNull() ? Process.findRangeByAddress(address) : null;
    if (!range || !range.protection.includes('x')) throw Error('Range not executable');
    return address;
  }
  function native(address, result, args) {
    const key=address.toString()+':'+result+':'+args.join(',');
    if(functionCache.has(key))return functionCache.get(key);
    const fn = new NativeFunction(executable(address), result, args, OPTIONS);
    const wrapper=(...values) => { nativeCalls++; return fn(...values); };
    functionCache.set(key,wrapper);return wrapper;
  }
  function slot(table, index) { return executable(readable(table.add(index*8),8).readPointer()); }
  function bind(table, index, result, args) { return native(slot(table,index), result, args); }
  const art = Process.findModuleByName('libart.so');
  const address = art?.findExportByName('JNI_GetCreatedJavaVMs') ?? Module.findGlobalExportByName('JNI_GetCreatedJavaVMs');
  if (!address) throw Error('JNI_GetCreatedJavaVMs unavailable');
  const getVMs = native(address,'int',['pointer','int','pointer']);
  const output = Memory.alloc(8), count = Memory.alloc(4); output.writePointer(ptr(0)); count.writeS32(0);
  if (getVMs(output,1,count) !== 0 || count.readS32() !== 1) throw Error('JVM is not unique');
  const vm = readable(output.readPointer(),8), vmTable = readable(vm.readPointer(),8);
  const getEnv = bind(vmTable,6,'int',['pointer','pointer','int']);
  const attach = bind(vmTable,7,'int',['pointer','pointer','pointer']);
  const detach = bind(vmTable,5,'int',['pointer']);
  function fromEnv(env) {
    readable(env,8); const table = readable(env.readPointer(),8), functions = new Map(), refs = [];
    function fn(index,result,args) {
      if (!functions.has(index)) functions.set(index,bind(table,index,result,args));
      return functions.get(index);
    }
    const check = fn(228,'uint8',['pointer']), clear = fn(17,'void',['pointer']);
    const deleteLocal = fn(23,'void',['pointer','pointer']);
    const cstr = text => Memory.allocUtf8String(text);
    function pending() { return check(env) !== 0; }
    function own(label, action) {
      if (pending()) throw Error('Preexisting JNI exception: '+label+'; not cleared');
      let value;
      try { value = action(); }
      catch (error) {
        if (pending()) { clear(env); report('jni_own_exception_cleared',{operation:label}); }
        throw error;
      }
      if (pending()) {
        clear(env); report('jni_own_exception_cleared',{operation:label});
        throw Error('Owned exception during '+label);
      }
      return value;
    }
    if (!pending()) {
      const capacity = own('EnsureLocalCapacity',()=>fn(26,'int',['pointer','int'])(env,64));
      if (capacity !== 0) throw Error('No capacity for JNI local references');
    }
    function track(ref) { if (ref && !ref.isNull()) refs.push(ref); return ref; }
    function nonNull(ref,label) { if (!ref || ref.isNull()) throw Error(label+' returned null'); return ref; }
    function findClass(name) {
      return nonNull(track(own('FindClass:'+name,()=>fn(6,'pointer',['pointer','pointer'])(env,cstr(name)))),name);
    }
    function method(clazz,name,signature,static_=false) {
      return nonNull(own('GetMethodID:'+name,()=>fn(static_?113:33,'pointer',
        ['pointer','pointer','pointer','pointer'])(env,clazz,cstr(name),cstr(signature))),name);
    }
    function staticField(clazz,name,signature) {
      return nonNull(own('GetStaticFieldID:'+name,()=>fn(144,'pointer',
        ['pointer','pointer','pointer','pointer'])(env,clazz,cstr(name),cstr(signature))),name);
    }
    function staticObject(clazz,id) {
      return track(own('GetStaticObjectField',()=>fn(145,'pointer',
        ['pointer','pointer','pointer'])(env,clazz,id)));
    }
    function callObject(object,id,args=ptr(0),static_=false) {
      return track(own('CallObjectMethodA',()=>fn(static_?116:36,'pointer',
        ['pointer','pointer','pointer','pointer'])(env,object,id,args)));
    }
    function scalar(object,id,type) {
      const index={boolean:39,int:51,long:54,float:57,double:60}[type];
      if (!index) throw Error('Invalid JNI type');
      return own('Location/enum getter',()=>fn(index,type==='boolean'?'uint8':type==='long'?'int64':type,
        ['pointer','pointer','pointer','pointer'])(env,object,id,ptr(0)));
    }
    function text(ref) {
      if (!ref || ref.isNull()) return null;
      const length = own('GetStringUTFLength',()=>fn(168,'int',['pointer','pointer'])(env,ref));
      if (length < 0 || length > 512) return {unread:true,bytes:length,reason:'string_sample_limit'};
      const chars = nonNull(own('GetStringUTFChars',()=>fn(169,'pointer',
        ['pointer','pointer','pointer'])(env,ref,ptr(0))), 'GetStringUTFChars');
      try { readable(chars,Math.max(1,length)); return chars.readUtf8String(length); }
      finally { fn(170,'void',['pointer','pointer','pointer'])(env,ref,chars); }
    }
    function loadApplicationClass(name) {
      const activity = findClass('android/app/ActivityThread');
      const application = nonNull(callObject(activity,method(activity,'currentApplication','()Landroid/app/Application;',true),ptr(0),true),'Application');
      const appClass = nonNull(track(own('GetObjectClass',()=>fn(31,'pointer',['pointer','pointer'])(env,application))),'Application class');
      const loader = nonNull(callObject(application,method(appClass,'getClassLoader','()Ljava/lang/ClassLoader;')),'ClassLoader');
      const loaderClass = findClass('java/lang/ClassLoader');
      const value = nonNull(track(own('NewStringUTF',()=>fn(167,'pointer',['pointer','pointer'])(env,cstr(name)))),'class name');
      const args = Memory.alloc(8); args.writePointer(value);
      return nonNull(callObject(loader,method(loaderClass,'loadClass','(Ljava/lang/String;)Ljava/lang/Class;'),args),'Application class');
    }
    function sdk() {
      const clazz = findClass('android/os/Build$VERSION');
      const id = nonNull(own('SDK_INT field',()=>fn(144,'pointer',
        ['pointer','pointer','pointer','pointer'])(env,clazz,cstr('SDK_INT'),cstr('I'))),'SDK_INT');
      return own('SDK_INT read',()=>fn(150,'int',['pointer','pointer','pointer'])(env,clazz,id));
    }
    function enumNames(clazz) {
      const values = nonNull(callObject(clazz,method(clazz,'values','()[Lcom/nianticlabs/nia/contextservice/ServiceStatus;',true),ptr(0),true),'ServiceStatus values');
      const size = own('GetArrayLength',()=>fn(171,'int',['pointer','pointer'])(env,values));
      if (size < 1 || size > 32) throw Error('Enum out of bounds');
      const enumClass = findClass('java/lang/Enum'), name = method(enumClass,'name','()Ljava/lang/String;'), ordinal = method(enumClass,'ordinal','()I');
      const names = {};
      for (let i=0;i<size;i++) {
        const value = nonNull(track(own('GetObjectArrayElement',()=>fn(173,'pointer',['pointer','pointer','int'])(env,values,i))),'enum value');
        names[scalar(value,ordinal,'int')] = text(callObject(value,name));
      }
      return names;
    }
    function retain(clazz) {
      return nonNull(own('NewGlobalRef',()=>fn(21,'pointer',['pointer','pointer'])(env,clazz)),'global class ref');
    }
    function release(ref) { fn(22,'void',['pointer','pointer'])(env,ref); }
    function prepareLocationRead() {
      for(const [index,type] of [[39,'uint8'],[54,'int64'],[57,'float'],[60,'double']])
        fn(index,type,['pointer','pointer','pointer','pointer']);
      fn(168,'int',['pointer','pointer']);fn(169,'pointer',['pointer','pointer','pointer']);fn(170,'void',['pointer','pointer','pointer']);
    }
    return {env, table, pending, own, findClass, method, staticField, staticObject, scalar, text, loadApplicationClass, sdk, enumNames,
      retain, release, prepareLocationRead,
      close() { const errors=[]; for (const ref of refs.splice(0).reverse()) {
        try { deleteLocal(env,ref); } catch(error) { errors.push(String(error)); }
      } return errors; }};
  }
  function withEnv(action) {
    const out = Memory.alloc(8); out.writePointer(ptr(0)); let ours = false, scope;
    const status = getEnv(vm,out,0x10006);
    if (status === -2) {
      if (attach(vm,out,ptr(0)) !== 0) throw Error('Could not attach owned thread');
      ours = true;
    } else if (status !== 0) throw Error('GetEnv: '+status);
    try { scope=fromEnv(out.readPointer()); if(scope.pending())throw Error('Preexisting JNI exception; not cleared'); return action(scope); }
    finally {
      const errors = scope ? scope.close() : [];
      if (ours && detach(vm) !== 0) errors.push('DetachCurrentThread failed');
      report('jni_scope_released',{ownThread:ours,errors,passed:errors.length===0});
      if (errors.length) throw Error('Could not release owned JNI scope');
    }
  }
  return {withEnv, fromEnv, readable, executable, slot, get nativeCalls(){return nativeCalls;}};
}
module.exports = {jniRuntime};
