'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {resolveNativeEntry,sampleGate}=require('../location/trace-policy.cjs');
function art(options={}) {
  const memory=new Map(),base=0x7e0000000000n;
  class Pointer {
    constructor(n){this.value=n instanceof Pointer?n.value:BigInt(n);}
    add(n){return new Pointer(this.value+BigInt(n));} and(other){return new Pointer(this.value&other.value);}
    isNull(){return this.value===0n;}equals(p){return this.value===p.value;}
    compare(p){return this.value<p.value?-1:this.value>p.value?1:0;}
    readU32(){return memory.get(this.value)??0;}
    readPointer(){return memory.get(this.value)??new Pointer(0);}
  }
  const ptr=n=>new Pointer(n),method=ptr(base+0x1000n),module={name:'libNianticLabsPlugin.so',base:ptr(base+0x10000n),size:0x10000};
  memory.set(method.add(4).value,options.flags??0x102);memory.set(method.add(24).value,ptr(base+BigInt(options.entryOffset??0x12000)));
  const r={ptr,Process:{arch:options.arch??'arm64',pointerSize:options.size??8}};
  const jni={readable:()=>{if(options.unreadable)throw Error('not readable');},executable:()=>{if(options.nonExecutable)throw Error('not executable');}};
  return {r,jni,method,module};
}
test('only exact Android 10 ART native data offset is used',()=>{
  const h=art(),out=resolveNativeEntry(h.r,h.jni,h.method,h.module,29);
  assert.equal(out.offset,24);assert.equal(out.flags,0x102);assert.ok(out.entry.equals(h.method.add(24).readPointer()));
});
test('other SDKs/ABIs/pointer widths are not guessed',()=>{
  for(const options of [{},{arch:'arm'},{size:4}]) {
    const h=art(options);const sdks=Object.keys(options).length?[29]:[28,30,31,35,36];
    for(const sdk of sdks)assert.throws(()=>resolveNativeEntry(h.r,h.jni,h.method,h.module,sdk));
  }
});
test('non-native, public, or static ART methods are rejected',()=>{
  for(const flags of [0,2,0x100,0x109,0x10a]){const h=art({flags});assert.throws(()=>resolveNativeEntry(h.r,h.jni,h.method,h.module,29));}
});
test('absent/wrong modules and out-of-module or inaccessible pointers are rejected',()=>{
  const h=art();assert.throws(()=>resolveNativeEntry(h.r,h.jni,h.method,null,29));
  assert.throws(()=>resolveNativeEntry(h.r,h.jni,h.method,{...h.module,name:'libart.so'},29));
  for(const options of [{entryOffset:0},{entryOffset:0x20000},{unreadable:true},{nonExecutable:true}]) {
    const item=art(options);assert.throws(()=>resolveNativeEntry(item.r,item.jni,item.method,item.module,29));
  }
});
test('encoded/tagged/unaligned or null method IDs do not cause memory scans',()=>{
  const h=art();for(const id of [h.r.ptr(0),h.r.ptr(1),h.method.add(1)])assert.throws(()=>resolveNativeEntry(h.r,h.jni,id,h.module,29));
});
test('first three samples and later throttling are independent for every status/method',()=>{
  let now=10000;const gate=sampleGate(()=>now);
  assert.ok(gate('status:0'));assert.ok(gate('status:0'));assert.ok(gate('status:0'));assert.equal(gate('status:0'),false);
  assert.ok(gate('status:1'));now+=1499;assert.equal(gate('status:0'),false);now++;assert.ok(gate('status:0'));
});
