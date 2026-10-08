/** Pointer-width GC handles for the inspected GO 0.431.0 / Unity 6000.3.21f1 ARM64 ABI.
 * Does not patch the bridge or use its uint32 GCHandle helpers.
 */
export type GcApi64 = {
  create:(object:NativePointer,pinned:boolean)=>NativePointer;
  createWeak:(object:NativePointer,trackResurrection:boolean)=>NativePointer;
  target:(handle:NativePointer)=>NativePointer;
  free:(handle:NativePointer)=>void;
};

export function createGcApi64(module:Pick<Module,'getExportByName'>):GcApi64 {
  if(Process.arch!=='arm64'||Process.pointerSize!==8)throw Error('GCHandle64 requires ARM64 and 8-byte pointers');
  const create=new NativeFunction(module.getExportByName('il2cpp_gchandle_new'),'pointer',['pointer','bool']);
  const createWeak=new NativeFunction(module.getExportByName('il2cpp_gchandle_new_weakref'),'pointer',['pointer','bool']);
  const target=new NativeFunction(module.getExportByName('il2cpp_gchandle_get_target'),'pointer',['pointer']);
  const free=new NativeFunction(module.getExportByName('il2cpp_gchandle_free'),'void',['pointer']);
  return {create:(object,pinned)=>create(object,+pinned),
    createWeak:(object,track)=>createWeak(object,+track),target:handle=>target(handle),free:handle=>{free(handle);}};
}

function pointer(value:unknown,label:string):NativePointer {
  // Never repair/guess truncated handles, cast them to Number, or use toInt32.
  if(!(value instanceof NativePointer))throw Error(label+': incompatible ABI; expected a 64-bit NativePointer');
  if(value.isNull())throw Error(label+': null reference');
  return value;
}

export class HeldReference64 {
  private releaseAttempted=false;
  private released=false;
  constructor(readonly token:NativePointer,readonly original:NativePointer,readonly label:string,
    private api:GcApi64,private onReleased:()=>void) { }
  get target():NativePointer {
    if(this.releaseAttempted)throw Error(this.label+': reference already released or release uncertain');
    return pointer(this.api.target(this.token),this.label+'.target');
  }
  assertIdentity():void {
    if(!this.target.equals(this.original))throw Error(this.label+': GCHandle returned a different object');
  }
  free():void {
    if(this.releaseAttempted) {
      if(!this.released)throw Error(this.label+': previous release uncertain; not retried');
      return;
    }
    this.releaseAttempted=true;
    this.api.free(this.token);
    this.released=true;this.onReleased();
  }
}

export class GcReferences64 {
  private owned=new Set<HeldReference64>();
  private closed=false;
  constructor(private api:GcApi64) { }
  get count():number {return this.owned.size;}
  retain(object:NativePointer,label:string,weak=false):HeldReference64 {
    if(this.closed)throw Error('Reference manager is closed');
    const original=pointer(object,label+'.object');
    const token=pointer(weak?this.api.createWeak(original,false):this.api.create(original,true),label+'.new');
    const reference=new HeldReference64(token,original,label,this.api,()=>this.owned.delete(reference));
    this.owned.add(reference);
    try {reference.assertIdentity();return reference;}
    catch(error) {
      try {reference.free();}catch(cleanup){throw Error(String(error)+'; cleanup: '+String(cleanup));}
      throw error;
    }
  }
  validateCached():number {
    for(const reference of this.owned){reference.assertIdentity();reference.assertIdentity();}
    return this.owned.size;
  }
  probe(object:NativePointer):{strongProbe:boolean;weakProbe:boolean;probeFreed:boolean;roundTrips:number} {
    const before=this.count;
    for(const weak of [false,true]) {
      const held=this.retain(object,weak?'probe:weak':'probe:strong',weak);
      try {held.assertIdentity();held.assertIdentity();}finally {held.free();}
    }
    if(this.count!==before)throw Error('Reference probe left a pending handle');
    return {strongProbe:true,weakProbe:true,probeFreed:true,roundTrips:4};
  }
  close():{passed:boolean;remaining:number;errors:string[]} {
    this.closed=true;
    const errors:string[]=[];
    // Continue releasing independent references if one failed. A failed free
    // remains counted, and is not blindly retried or reported as successful.
    for(const reference of [...this.owned])try {reference.free();}catch(error){errors.push(String(error));}
    return {passed:errors.length===0&&this.count===0,remaining:this.count,errors};
  }
}
