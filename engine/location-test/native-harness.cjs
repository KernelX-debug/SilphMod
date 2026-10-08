'use strict';
const assert=require('node:assert/strict');

// Simulates only the public JNI/Frida contracts used by our code. No Android,
// commercial code or ELF is evaluated by the tests.
function harness(options={}) {
  const mem=new Map(),bodies=new Map(),hooks=new Map(),classes=new Map(),methodNames=new Map(),calls=[],events=[];
  const base=0x7e4400000000n;let next=base+0x80000n,pending=!!options.preexistingException,attachCount=0,globalCount=0;
  class Pointer {
    constructor(value){this.value=value instanceof Pointer?value.value:BigInt(value);}
    add(n){return new Pointer(this.value+BigInt(n));}sub(other){return new Pointer(this.value-other.value);}
    and(other){return new Pointer(this.value&other.value);}isNull(){return this.value===0n;}
    compare(other){return this.value<other.value?-1:this.value>other.value?1:0;}equals(other){return this.value===other.value;}
    toString(){return '0x'+this.value.toString(16);}toUInt32(){return Number(this.value&0xffffffffn);}
    toInt32(){return Number(BigInt.asIntN(32,this.value));}replace(other){this.value=other.value;}
    writePointer(value){mem.set(this.toString(),value);}readPointer(){assert.ok(mem.has(this.toString()),'uninitialized pointer '+this);return mem.get(this.toString());}
    writeS32(value){mem.set(this.toString(),value);}readS32(){return mem.get(this.toString())??0;}
    readU32(){return mem.get(this.toString())??0;}
    readUtf8String(length){const text=mem.get(this.toString());assert.equal(typeof text,'string');return length===undefined?text:text.slice(0,length);}
  }
  const ptr=v=>new Pointer(v),address=n=>ptr(base+BigInt(n));
  function alloc(size=8){next=(next+7n)&~7n;const out=ptr(next);next+=BigInt(size+16);return out;}
  const cstr=s=>{const out=alloc(s.length+1);mem.set(out.toString(),s);return out;};
  const nameOf=p=>mem.get(p.toString());
  const vm=address(0x100),vmTable=address(0x1000),env=address(0x200),table=address(0x2000);
  vm.writePointer(vmTable);env.writePointer(table);
  const plugin={name:'libNianticLabsPlugin.so',base:address(0x50000),size:0x10000};
  const nativeEntry=address(0x51000);
  function body(offset,name,fn){const out=address(offset);bodies.set(out.toString(),{name,fn});return out;}
  function slot(table,index,name,fn){const out=body((table===vmTable?0x10000:0x20000)+index*32,name,fn);table.add(index*8).writePointer(out);return out;}
  function clazz(name){if(!classes.has(name))classes.set(name,alloc());return classes.get(name);}
  const methodByName=new Map();
  function method(name) {
    if(!methodByName.has(name)) {
      const out=alloc(48);methodByName.set(name,out);methodNames.set(out.toString(),name);
      if(name==='nativeLocationUpdate'){mem.set(out.add(4).toString(),options.flags??0x102);out.add(24).writePointer(options.outsideModule?address(0x61000):nativeEntry);}
    }
    return methodByName.get(name);
  }
  const array=alloc(),enums=[alloc(),alloc(),alloc()],app=alloc(),loader=alloc();
  const enumTitles=['UNDEFINED','RUNNING','FAILED'];
  const getVMs=body(0x9000,'GetVMs',(out,limit,count)=>{assert.equal(limit,1);out.writePointer(vm);count.writeS32(1);return 0;});
  slot(vmTable,6,'GetEnv',(_vm,out,version)=>{assert.equal(version,0x10006);out.writePointer(env);return options.detached?-2:0;});
  slot(vmTable,7,'AttachDaemon',(_vm,out)=>{out.writePointer(env);return 0;});
  slot(vmTable,5,'DetachThread',()=>options.detachFail?-1:0);
  slot(table,228,'ExceptionCheck',()=>pending?1:0);
  slot(table,17,'ExceptionClear',()=>{pending=false;});
  slot(table,26,'EnsureCapacity',(_env,n)=>{assert.equal(n,64);return options.capacityFail?-1:0;});
  slot(table,23,'DeleteLocal',()=>{});
  slot(table,21,'NewGlobal',(_env,obj)=>{globalCount++;return obj;});
  slot(table,22,'DeleteGlobal',()=>{globalCount--;});
  slot(table,6,'FindClass',(_env,name)=>clazz(nameOf(name)));
  const getMethod=(_env,_class,name,_sig)=>{
    const value=nameOf(name);
    if(value==='isMock'||options.missingMock&&value==='isFromMockProvider'){pending=true;return ptr(0);}
    return method(value);
  };
  slot(table,33,'GetMethod',getMethod);slot(table,113,'GetStaticMethod',getMethod);
  slot(table,31,'ObjectClass',()=>clazz('Application'));
  slot(table,144,'GetStaticField',()=>alloc());slot(table,150,'GetStaticInt',()=>options.sdk??29);
  slot(table,167,'NewString',(_env,value)=>cstr(nameOf(value)));
  function callObject(_env,obj,id,args) {
    const name=methodNames.get(id.toString());
    if(name==='currentApplication')return app;
    if(name==='getClassLoader')return loader;
    if(name==='loadClass')return clazz(nameOf(args.readPointer()));
    if(name==='values')return array;
    if(name==='name')return cstr(enumTitles[enums.findIndex(e=>e.equals(obj))]);
    throw Error('Unexpected object getter '+name);
  }
  slot(table,36,'ObjectA',callObject);slot(table,116,'StaticObjectA',callObject);
  slot(table,168,'StringLength',(_env,obj)=>Buffer.byteLength(nameOf(obj)));
  slot(table,169,'StringChars',(_env,obj)=>obj);slot(table,170,'ReleaseString',()=>{});
  slot(table,171,'ArrayLength',()=>3);slot(table,173,'ArrayElement',(_env,_array,i)=>enums[i]);
  slot(table,51,'IntA',(_env,obj,id)=>{assert.equal(methodNames.get(id.toString()),'ordinal');return enums.findIndex(e=>e.equals(obj));});
  slot(table,54,'LongA',()=>({toString:()=> '12345678901234567'}));
  slot(table,57,'FloatA',()=>5);
  slot(table,60,'DoubleA',(_env,_obj,id)=>methodNames.get(id.toString())==='getLongitude'?-100.5:22.1);
  for(const index of [37,38,39])slot(table,index,'Boolean'+index,()=>1);
  if(options.aliases)for(const index of [38,39])table.add(index*8).writePointer(table.add(37*8).readPointer());
  function invoke(address,args,fn,context={}) {
    const callback=hooks.get(address.toString());const ctx={returnAddress:plugin.base.add(0x2000),context:{},...context};
    callback?.onEnter?.call(ctx,args);let result=fn(...args);
    if(callback?.onLeave){const raw=result instanceof Pointer?result:ptr(typeof result==='number'?result:0);callback.onLeave.call(ctx,raw);result=raw.toUInt32();}
    return result;
  }
  const runtime={ptr,Process:{id:123,arch:'arm64',pointerSize:8,getCurrentThreadId:()=>22,
    findRangeByAddress(p){return p.isNull()?null:{base:address(0),size:0x100000,protection:'rwx'};},
    findModuleByName(name){return name==='libart.so'?{findExportByName:()=>getVMs}:name===plugin.name?plugin:null;},
    findModuleByAddress:()=>plugin},Module:{findGlobalExportByName:()=>null},
    Memory:{alloc,allocUtf8String:cstr},
    NativeFunction:function(address,_result,_args,opts){
      assert.equal(opts.exceptions,'propagate');assert.equal(opts.scheduling,'cooperative');assert.equal(opts.traps,'none');
      assert.ok(bodies.has(address.toString()),'unexpected JNI slot '+address);
      return (...args)=>{const {name,fn}=bodies.get(address.toString());calls.push(name);return fn(...args);};
    },
    Interceptor:{attach(address,callbacks){attachCount++;if(attachCount===options.failAttach)throw Error('attach failed');
      assert.ok(!hooks.has(address.toString()));hooks.set(address.toString(),callbacks);
      return {detach(){if(options.failDetach)throw Error('detach failed');hooks.delete(address.toString());}};
    }},Thread:{backtrace:()=>{calls.push('Backtrace');if(options.backtraceThrows)throw Error('backtrace unavailable');return [plugin.base.add(0x2000)];}},Backtracer:{ACCURATE:1},DebugSymbol:{fromAddress:p=>({toString:()=>plugin.name+'!'+p})}};
  function boolean(before=1,name='isFromMockProvider',index=39) {
    return invoke(table.add(index*8).readPointer(),[env,alloc(),method(name)],()=>before);
  }
  function update(ordinal=1,nullLocation=false) {
    const callback=hooks.get(nativeEntry.toString());assert.ok(callback,'native observer missing');
    const context={returnAddress:plugin.base.add(0x2000),context:{}};
    const args=[env,alloc(),cstr('fused'),nullLocation?ptr(0):alloc(),ptr(ordinal),alloc()];
    const original=args.map(p=>p.toString());
    callback.onEnter.call(context,args);
    assert.deepEqual(args.map(p=>p.toString()),original,'observer changed native JNI arguments');
    callback.onLeave.call(context,ptr(0));return context;
  }
  return {runtime,events,calls,hooks,boolean,update,report:(e,d)=>events.push({event:e,...d}),
    setException:value=>{pending=value;},get globalCount(){return globalCount;}};
}
module.exports={harness};
