const test=require('node:test'),assert=require('node:assert/strict');
const vm=require('node:vm'),esbuild=require('esbuild');
const source=esbuild.buildSync({entryPoints:['src/gc-handles.ts'],bundle:true,write:false,
  format:'cjs',platform:'neutral'}).outputFiles[0].text;

function harness(options={}) {
  const objects=new Map(),names=new Map(),bindings=[],calls=[];
  let next=0x7e4474a02828n;
  class Pointer {
    constructor(value){this.value=BigInt(value);}
    isNull(){return this.value===0n;}
    equals(other){return other instanceof Pointer&&this.value===other.value;}
    toString(){return '0x'+this.value.toString(16);}
    toInt32(){throw Error('Truncation forbidden');}
    valueOf(){throw Error('Number conversion forbidden');}
  }
  const module={getExportByName(name){const address=new Pointer(BigInt(names.size+1));names.set(address.toString(),name);return address;}};
  function NativeFunction(address,returnType,argTypes) {
    const name=names.get(address.toString());bindings.push({name,returnType,argTypes:Array.from(argTypes)});
    return (...args)=>{
      assert.ok(args[0] instanceof Pointer);calls.push({name,argument:args[0].toString()});
      if(name==='il2cpp_gchandle_new'||name==='il2cpp_gchandle_new_weakref') {
        const token=new Pointer(next);next+=8n;objects.set(token.toString(),args[0]);return token;
      }
      const key=args[0].toString();assert.ok(objects.has(key),'Unknown/truncated/freed handle '+key);
      if(name==='il2cpp_gchandle_get_target')return options.wrongTarget?new Pointer(0x7e1111111111n):objects.get(key);
      if(key===options.failFree)throw Error('Native free failed');
      objects.delete(key);
    };
  }
  const sandbox={module:{exports:{}},NativePointer:Pointer,NativeFunction,
    Process:{arch:options.arch??'arm64',pointerSize:options.pointerSize??8}};
  vm.runInNewContext(source,sandbox);
  const library=sandbox.module.exports;
  return {library,Pointer,module,objects,bindings,calls,options,
    manager(){return new library.GcReferences64(library.createGcApi64(module));}};
}

test('all four native GC signatures preserve pointer-width handles',()=>{
  const h=harness();const manager=h.manager();
  assert.deepEqual(h.bindings.map(x=>[x.name,x.returnType,x.argTypes]),[
    ['il2cpp_gchandle_new','pointer',['pointer','bool']],
    ['il2cpp_gchandle_new_weakref','pointer',['pointer','bool']],
    ['il2cpp_gchandle_get_target','pointer',['pointer']],
    ['il2cpp_gchandle_free','void',['pointer']]
  ]);
  const object=new h.Pointer(0x7eabcdef1234n),held=manager.retain(object,'avatar');
  assert.ok(held.token.value>0xffffffffn);assert.ok(held.target.equals(object));
  held.assertIdentity();held.free();assert.equal(manager.count,0);
  for(const call of h.calls)assert.ok(BigInt(call.argument)>0xffffffffn);
});

test('strong and weak probe references are recovered repeatedly and freed',()=>{
  const h=harness(),manager=h.manager();
  const object=new h.Pointer(0x7eabcdef1234n);manager.retain(object,'avatar');
  const result=manager.probe(object);
  assert.ok(result.strongProbe&&result.weakProbe&&result.probeFreed);assert.equal(result.roundTrips,4);
  assert.equal(manager.count,1);assert.equal(manager.validateCached(),1);
  assert.equal(manager.close().remaining,0);assert.equal(h.objects.size,0);
});

test('free is idempotent and use after free is rejected without native calls',()=>{
  const h=harness(),manager=h.manager(),held=manager.retain(new h.Pointer(0x7eabcdef1234n),'avatar');
  held.free();held.free();assert.equal(h.calls.filter(x=>x.name==='il2cpp_gchandle_free').length,1);
  const calls=h.calls.length;assert.throws(()=>held.target,/reference already released/);assert.equal(h.calls.length,calls);
});

test('wrong round-trip identity releases the newly created reference and fails',()=>{
  const h=harness({wrongTarget:true}),manager=h.manager();
  assert.throws(()=>manager.retain(new h.Pointer(0x7eabcdef1234n),'avatar'),/returned a different object/);
  assert.equal(manager.count,0);assert.equal(h.objects.size,0);
});

test('failed free is not retried, stays unconfirmed, and other references are freed',()=>{
  const h=harness(),manager=h.manager();
  const a=manager.retain(new h.Pointer(0x7eabcdef1234n),'avatar');
  manager.retain(new h.Pointer(0x7eabcdef5678n),'promise');h.options.failFree=a.token.toString();
  const result=manager.close();assert.equal(result.passed,false);assert.equal(result.remaining,1);
  assert.equal(h.objects.size,1);assert.equal(h.calls.filter(x=>x.name==='il2cpp_gchandle_free').length,2);
  assert.throws(()=>a.free(),/not retried/);assert.throws(()=>a.target,/uncertain/);
  assert.equal(h.calls.filter(x=>x.name==='il2cpp_gchandle_free').length,2);
  assert.throws(()=>manager.retain(new h.Pointer(0x7eabcdef9999n),'new'),/Reference manager is closed/);
});

test('unsupported architecture/width is rejected before any native binding',()=>{
  for(const options of [{arch:'arm'},{pointerSize:4}]) {
    const h=harness(options);assert.throws(()=>h.manager(),/ARM64 and 8-byte pointers/);assert.equal(h.bindings.length,0);
  }
});
