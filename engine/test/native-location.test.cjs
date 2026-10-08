const test = require('node:test'), assert = require('node:assert/strict');
const {installNativeLocation} = require('../src/native-location.cjs');

function harness(options={}) {
  const memory = new Map(), functions = new Map(), hooks = new Map(), events = [], calls = [];
  const base = 0x7e4400000000n;
  let allocation = base+0x80000n, pendingException=!!options.preexistingException, installed=0;
  class Pointer {
    constructor(value) {this.value=value instanceof Pointer?value.value:BigInt(value);}
    isNull(){return this.value===0n;}
    add(value){return new Pointer(this.value+BigInt(value));}
    compare(other){return this.value<other.value?-1:this.value>other.value?1:0;}
    equals(other){return this.value===other.value;}
    toString(){return '0x'+this.value.toString(16);}
    readPointer(){assert.ok(memory.has(this.toString()),'Pointer read not initialized');return memory.get(this.toString());}
    writePointer(value){memory.set(this.toString(),value);}
    readS32(){return memory.get(this.toString())??0;}
    writeS32(value){memory.set(this.toString(),value);}
    toUInt32(){return Number(this.value&0xffffffffn);}
    replace(value){this.value=value.value;}
  }
  const ptr=value=>new Pointer(value), address=offset=>ptr(base+BigInt(offset));
  const vm=address(0x100), vmTable=address(0x1000), env=address(0x200), table=address(0x2000), classRef=address(0x300);
  vm.writePointer(vmTable); env.writePointer(table);
  const methods=new Map([['isFromMockProvider',address(0x11110)],['isMock',address(0x11120)]]);
  const stringAt=pointer=>memory.get(pointer.toString());
  function functionAt(offset,name,body) {
    const pointer=address(offset);
    functions.set(pointer.toString(),(...args)=>{calls.push({name,args});return body(...args);});
    return pointer;
  }
  function slot(table,index,name,body) {
    const pointer=functionAt((table===vmTable?0x20000:0x30000)+index*32,name,body);
    table.add(index*8).writePointer(pointer);
    return pointer;
  }
  const getVMs=functionAt(0x10000,'GetCreatedVMs',(_out,limit,count)=>{
    assert.equal(limit,1); _out.writePointer(vm); count.writeS32(options.vmCount??1);return options.vmStatus??0;
  });
  slot(vmTable,6,'GetEnv',(v,out,version)=>{assert.ok(v.equals(vm));assert.equal(version,0x10006);out.writePointer(env);return options.envStatus??0;});
  slot(vmTable,7,'AttachAsDaemon',(v,out,args)=>{assert.ok(v.equals(vm));assert.ok(args.isNull());out.writePointer(env);return options.attachStatus??0;});
  slot(vmTable,5,'DetachThread',()=>options.detachStatus??0);
  slot(table,228,'ExceptionCheck',()=>pendingException?1:0);
  slot(table,17,'ExceptionClear',()=>{pendingException=false;});
  slot(table,23,'DeleteLocal',(_env,ref)=>{assert.ok(ref.equals(classRef));});
  slot(table,6,'FindClass',(_env,name)=>{assert.equal(stringAt(name),'android/location/Location');
    if(options.classException)pendingException=true;return options.classNull?ptr(0):classRef;});
  slot(table,33,'GetMethod',(_env,ref,name,signature)=>{
    assert.ok(ref.equals(classRef));assert.equal(stringAt(signature),'()Z');const method=stringAt(name);
    if(options.missing?.includes(method)){pendingException=true;return ptr(0);}
    return options.ambiguous?methods.get('isFromMockProvider'):methods.get(method);
  });
  for(const index of [37,38,39])slot(table,index,'Boolean'+index,()=>{});
  if(options.aliases)for(const index of [38,39])table.add(index*8).writePointer(table.add(37*8).readPointer());
  const runtime={ptr,Process:{arch:options.arch??'arm64',pointerSize:options.pointerSize??8,
    findRangeByAddress(pointer){if(pointer.isNull()||options.unreadable)return null;
      return {base:address(0),size:0x100000,protection:options.nonExecutable?'rw-':'rwx'};},
    findModuleByName(){return {findExportByName:()=>options.noExport?null:getVMs};}},
    Module:{findGlobalExportByName:()=>null},
    Memory:{alloc(bytes){const result=ptr(allocation);allocation+=BigInt(bytes+16);return result;},
      allocUtf8String(value){const result=ptr(allocation);allocation+=BigInt(value.length+16);memory.set(result.toString(),value);return result;}},
    NativeFunction:function(pointer,result,args,nativeOptions){
      assert.equal(nativeOptions.scheduling,'exclusive');assert.equal(nativeOptions.exceptions,'propagate');
      const fn=functions.get(pointer.toString());assert.ok(fn,'Unknown native binding');return fn;},
    Interceptor:{attach(pointer,callbacks){
      installed++;if(options.failAttach===installed)throw Error('Simulated attach failure');
      assert.ok(!hooks.has(pointer.toString()),'Duplicate hook');hooks.set(pointer.toString(),callbacks);
      return {detach(){if(options.failDetach)throw Error('Simulated detach failure');hooks.delete(pointer.toString());}};
    }},
  };
  function install(){return installNativeLocation(runtime,(event,data)=>events.push({event,...data}));}
  function enter(variant,method){const callback=hooks.get(table.add(variant*8).readPointer().toString());
    assert.ok(callback);const context={};callback.onEnter.call(context,[env,classRef,methods.get(method)??address(0x55550)]);return {callback,context};}
  function leave(invocation,before){const result=ptr(before);invocation.callback.onLeave.call(invocation.context,result);return result.toUInt32();}
  return {install,events,calls,hooks,enter,leave};
}

test('resolves Location boolean IDs and all three JNI variants with full-width pointers',()=>{
  const h=harness(), adapter=h.install();assert.equal(h.hooks.size,3);
  const ready=h.events.find(e=>e.event==='native_location_ready');
  assert.deepEqual(ready.methods,['isFromMockProvider','isMock']);
  for(const variant of [37,38,39])assert.equal(h.leave(h.enter(variant,'isMock'),1),0);
  assert.equal(adapter.close().changed,3);assert.equal(h.hooks.size,0);
});
test('Android 10 missing isMock is allowed without masking the resolution exception',()=>{
  const h=harness({missing:['isMock']});h.install();
  assert.deepEqual(h.events[0].missing,['isMock']);assert.equal(h.calls.filter(c=>c.name==='ExceptionClear').length,1);
});
test('unrelated JNI methods and original false results remain unchanged',()=>{
  const h=harness(), adapter=h.install();assert.equal(h.leave(h.enter(37,'unrelated'),1),1);
  assert.equal(h.leave(h.enter(38,'isFromMockProvider'),0),0);assert.equal(adapter.close().changed,0);
});
test('nested native calls keep per-invocation decisions independent',()=>{
  const h=harness();h.install();const outer=h.enter(37,'isFromMockProvider'), inner=h.enter(37,'unrelated');
  assert.equal(h.leave(inner,1),1);assert.equal(h.leave(outer,1),0);
});
test('callbacks in progress do not change a result after Stop',()=>{
  const h=harness(), adapter=h.install(), pending=h.enter(37,'isMock');adapter.close();assert.equal(h.leave(pending,1),1);
});
test('aliased JNI entries are hooked once',()=>{
  const h=harness({aliases:true}), adapter=h.install();assert.equal(h.hooks.size,1);assert.equal(adapter.close().passed,true);
});
test('a failed partial installation rolls back prior hooks and releases local references',()=>{
  const h=harness({failAttach:2});assert.throws(h.install,/attach failure/);assert.equal(h.hooks.size,0);
  assert.equal(h.calls.filter(c=>c.name==='DeleteLocal').length,1);
});
test('only our newly attached daemon thread is detached',()=>{
  const normal=harness();normal.install();assert.equal(normal.calls.filter(c=>c.name==='DetachThread').length,0);
  const own=harness({envStatus:-2});own.install();assert.equal(own.calls.filter(c=>c.name==='AttachAsDaemon').length,1);
  assert.equal(own.calls.filter(c=>c.name==='DetachThread').length,1);
});
test('thread-release failure rolls back hooks',()=>{
  const h=harness({envStatus:-2,detachStatus:-1});assert.throws(h.install,/release/);assert.equal(h.hooks.size,0);
});
test('Stop is idempotent and failed detach is not reported as successful',()=>{
  const h=harness(), adapter=h.install();assert.equal(adapter.close().passed,true);assert.equal(adapter.close().remaining,0);
  const failing=harness({failDetach:true}), faulty=failing.install().close();assert.equal(faulty.passed,false);assert.equal(faulty.remaining,3);
});
test('per-call logs are bounded and summary aggregates without coordinates',()=>{
  const h=harness(), adapter=h.install();for(let i=0;i<500;i++)h.leave(h.enter(39,'isMock'),1);
  assert.equal(h.events.filter(e=>e.event==='native_location_sample').length,6);
  adapter.summary(30000);adapter.summary(31000);adapter.summary(60000);
  assert.equal(h.events.filter(e=>e.event==='native_location_summary').length,2);
  const result=adapter.close();assert.equal(result.calls,500);assert.equal(result.changed,500);
  assert.equal(result.coordinateMutation,false);assert.equal(result.systemMutation,false);
});
for(const [label,options,expected] of [
  ['architecture',{arch:'arm'},/ARM64/],['pointer width',{pointerSize:4},/ARM64/],
  ['missing VM export',{noExport:true},/unavailable/],['VM count',{vmCount:0},/JVM/],
  ['VM error',{vmStatus:-1},/JVM/],['GetEnv error',{envStatus:-3},/GetEnv/],
  ['attach failure',{envStatus:-2,attachStatus:-1},/attach/],
  ['unreadable table',{unreadable:true},/executable|readable/],['nonexecutable entry',{nonExecutable:true},/executable/],
  ['missing class',{classNull:true},/class/],['class exception',{classException:true},/FindClass/],
  ['missing required method',{missing:['isFromMockProvider']},/required/i],['ambiguous IDs',{ambiguous:true},/Ambiguous/],
  ['preexisting exception',{preexistingException:true},/already has/],
])test('rejects '+label+' before any live hook remains',()=>{
  const h=harness(options);assert.throws(h.install,expected);assert.equal(h.hooks.size,0);
  if(options.preexistingException)assert.equal(h.calls.filter(c=>c.name==='ExceptionClear').length,0);
});
