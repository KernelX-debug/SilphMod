const test=require('node:test'),assert=require('node:assert/strict');
const ui=require('../src/ui-interruptions.cjs');

function harness(options={}) {
  let now=10000,permitted=true,observing=true,sameThread=true,callbacks,detached=0,reads=0,originals=0;
  const events=[],references=new Set(),calls=[],controllers=new Map();
  const enumType={name:ui.GUI_CLASS+'+OpenState',class:{isEnum:true,baseType:{name:'System.Int32'}}};
  function metadata(name) {
    const type={fullName:name,hierarchy:()=>[type,{fullName:ui.GUI_CLASS}],tryField(field){
      return field==='currentState'?{name:field,isStatic:false,offset:112,type:options.badState?{class:{isEnum:false}}:enumType}:
        field==='m_CachedPtr'?{name:field,isStatic:false,type:{name:'System.IntPtr'}}:null;
    },tryMethod(method){
      if(options.missingBase&&name===ui.GUI_CLASS)return null;
      if(options.missingAction===name&&method!==ui.OPEN_COMPLETED&&method!=='get_IsTopGui')return null;
      return {isStatic:options.staticMethod===method,parameterCount:options.badParameter===method?1:0,
        returnType:{name:method==='get_IsTopGui'?'System.Boolean':'System.Void'},
        virtualAddress:{isNull:()=>options.nullMethod===method},bind(controller){return {invoke(){
          reads++;
          if(method==='get_IsTopGui')return controller.top;
          calls.push({name:controller.class.fullName,method,originals});
          if(controller.throwAction)throw Error('action failure');
          options.onAction?.(controller,method);
          controller.state=method==='Click'?2:3;
        }};}};
    }};return type;
  }
  const runtime={resolve:metadata,object:pointer=>controllers.get(pointer),now:()=>now,
    isPointer:value=>value?.nativePointer===true,
    sameThread:()=>sameThread,canObserve:()=>observing,canAct:()=>permitted,
    number:value=>typeof value==='number'?value:value.field('value__').value,
    attach(_address,handlers){callbacks=handlers;return {detach(){detached++;if(options.detachFailure)throw Error('detach failure');}};},
    retain(controller){const reference={controller,released:false,free(){
      if(options.freeFailure)throw Error('free failure');
      assert.equal(this.released,false);this.released=true;references.delete(this);
    }};references.add(reference);return reference;},read:reference=>{reads++;assert.ok(references.has(reference));return reference.controller;}};
  const adapter=ui.install(runtime,(event,detail)=>events.push({event,...detail}));
  let next=0;
  function begin(name,extra={}) {
    const key='controller-'+(++next);
    const controller={class:{fullName:name},handle:{toString:()=>key},state:1,top:true,live:true,...extra,
      field(field){return {value:field==='currentState'?(options.enumValue?{field:()=>({value:this.state})}:this.state):
        {nativePointer:!extra.badPointer,isNull:()=>!this.live}};}};
    controllers.set(key,controller);const context={};callbacks?.onEnter.call(context,[key]);
    return {controller,context};
  }
  function finish(opening) {originals++;opening.controller.state=2;callbacks?.onLeave.call(opening.context);return opening.controller;}
  return {adapter,events,calls,references,begin,finish,open:(name,extra)=>finish(begin(name,extra)),
    completeAgain(controller){const context={};callbacks?.onEnter.call(context,[controller.handle.toString()]);finish({controller,context});},
    reopen(controller){controller.state=1;const context={};callbacks?.onEnter.call(context,[controller.handle.toString()]);finish({controller,context});},
    advance(ms){now+=ms;adapter.drain();},setPermission(value){permitted=value;},setObserving(value){observing=value;},
    setThread(value){sameThread=value;},get reads(){return reads;},get originals(){return originals;},get detached(){return detached;}};
}

test('all supported screens use their own validated lifecycle action after original opening',()=>{
  const h=harness();
  for(const target of ui.TARGETS) {
    const controller=h.open(target.name);
    const action=h.calls.at(-1);assert.equal(action.name,target.name);assert.equal(action.method,target.action);
    assert.equal(action.originals,h.originals);assert.equal(controller.state,target.action==='Click'?2:3);
  }
  assert.equal(h.calls.length,ui.TARGETS.length);assert.equal(h.references.size,0);
  assert.equal(h.events.filter(e=>e.event==='ui_interruption_result').length,ui.TARGETS.length);
  assert.ok(h.adapter.close().passed);assert.equal(h.detached,1);
});

test('enum values from IL2CPP are read as their underlying integer rather than enum names',()=>{
  const h=harness({enumValue:true});h.open(ui.TARGETS[0].name);
  assert.equal(h.calls.length,1);assert.ok(h.adapter.close().passed);
});

test('a service response, general modal, manual collection and unrelated subclass are never dismissed',()=>{
  const h=harness();
  for(const name of ['Holoholo.Rpc.LevelUpRewardsOutProto','Niantic.Holoholo.UI.ModalDialog',
    'Niantic.Holoholo.UI.PokemonInventoryGuiController',ui.TARGETS[0].name+'Extra'])h.open(name);
  assert.equal(h.calls.length,0);assert.equal(h.references.size,0);assert.ok(h.adapter.close().passed);
});

test('disabled observation leaves read-only startup and other threads untouched',()=>{
  const h=harness();h.setObserving(false);h.open(ui.TARGETS[0].name);h.setObserving(true);
  h.setThread(false);h.open(ui.TARGETS[0].name);
  assert.equal(h.references.size,0);assert.equal(h.calls.length,0);assert.ok(h.adapter.close().passed);
});

test('focus loss defers the action and a later permitted Unity-thread callback resumes it',()=>{
  const h=harness();h.setPermission(false);h.open(ui.TARGETS[0].name);
  assert.equal(h.calls.length,0);assert.equal(h.references.size,1);
  h.setPermission(true);h.setThread(false);h.advance(1000);assert.equal(h.calls.length,0);
  h.setThread(true);h.advance(250);assert.equal(h.calls.length,1);assert.equal(h.references.size,0);
  assert.ok(h.adapter.close().passed);
});

test('only the top GUI is dismissed and other queued views wait without losing references',()=>{
  const h=harness();const controller=h.open(ui.TARGETS[0].name,{top:false});assert.equal(h.calls.length,0);
  controller.top=true;h.advance(250);assert.equal(h.calls.length,1);assert.equal(h.references.size,0);
  assert.ok(h.adapter.close().passed);
});

test('a destroyed or already-dismissed controller releases its reference without another action',()=>{
  for(const change of [controller=>controller.live=false,controller=>controller.state=3]) {
    const h=harness();h.setPermission(false);const controller=h.open(ui.TARGETS[0].name);
    change(controller);h.setPermission(true);h.advance(250);
    assert.equal(h.calls.length,0);assert.equal(h.references.size,0);assert.ok(h.adapter.close().passed);
  }
});

test('a boxed pointer address is not mistaken for a live Unity native object',()=>{
  const h=harness();h.open(ui.TARGETS[0].name,{badPointer:true});
  assert.equal(h.calls.length,0);assert.equal(h.references.size,0);assert.equal(h.adapter.snapshot().failures,1);
  assert.ok(h.adapter.close().passed);
});

test('controller identity replacement disables only its target and never calls its method',()=>{
  const h=harness();h.setPermission(false);const controller=h.open(ui.TARGETS[0].name);
  controller.class.fullName='DifferentController';h.setPermission(true);h.advance(250);
  assert.equal(h.calls.length,0);assert.equal(h.references.size,0);assert.equal(h.adapter.snapshot().failures,1);
  h.open(ui.TARGETS[1].name);assert.equal(h.calls.length,1);assert.ok(h.adapter.close().passed);
});

test('stale deferred actions expire without dismissing a screen on return',()=>{
  const h=harness();h.setPermission(false);h.open(ui.TARGETS[0].name);h.advance(ui.EXPIRY_MS);
  h.setPermission(true);h.advance(250);assert.equal(h.calls.length,0);assert.equal(h.references.size,0);
  assert.equal(h.adapter.snapshot().expired,1);assert.ok(h.adapter.close().passed);
});

test('the queue is bounded and drains no more than two actions per pass',()=>{
  const h=harness();h.setPermission(false);
  for(let i=0;i<100;i++)h.open(ui.TARGETS[0].name);
  assert.equal(h.references.size,ui.MAX_PENDING);assert.equal(h.adapter.snapshot().pending,ui.MAX_PENDING);
  h.setPermission(true);h.advance(250);assert.equal(h.calls.length,ui.MAX_ACTIONS);
  assert.ok(h.adapter.close().passed);assert.equal(h.references.size,0);
});

test('empty observer drains perform no GUI reads or retained-object work',()=>{
  const h=harness();const before=h.reads;for(let i=0;i<10000;i++)h.adapter.drain();
  assert.equal(h.reads,before);assert.equal(h.references.size,0);assert.equal(h.calls.length,0);
  assert.ok(h.adapter.close().passed);
});

test('missing methods or incompatible signatures leave the original GUI intact',()=>{
  for(const options of [{missingBase:true},{badState:true},{badParameter:ui.OPEN_COMPLETED},
    {staticMethod:ui.OPEN_COMPLETED},{nullMethod:ui.OPEN_COMPLETED}]) {
    const h=harness(options);h.open(ui.TARGETS[0].name);
    assert.equal(h.calls.length,0);assert.equal(h.references.size,0);
    assert.equal(h.adapter.snapshot().available,false);assert.ok(h.adapter.close().passed);
  }
});

test('one unavailable screen does not disable the rest of the observer',()=>{
  const h=harness({missingAction:ui.TARGETS[0].name});h.open(ui.TARGETS[0].name);h.open(ui.TARGETS[1].name);
  assert.equal(h.calls.length,1);assert.equal(h.adapter.snapshot().enabledTargets,ui.TARGETS.length-1);
  assert.ok(h.adapter.close().passed);
});

test('a failed action is not retried and farming-independent screens remain available',()=>{
  const h=harness();h.open(ui.TARGETS[0].name,{throwAction:true});h.open(ui.TARGETS[0].name);
  assert.equal(h.calls.length,1);assert.equal(h.references.size,0);h.open(ui.TARGETS[1].name);
  assert.equal(h.calls.length,2);assert.ok(h.events.some(e=>e.event==='ui_interruption_error'&&e.farmingContinues));
  assert.ok(h.adapter.close().passed);
});

test('Stop releases queued and in-flight references before a delayed native return',()=>{
  const h=harness();h.setPermission(false);h.open(ui.TARGETS[0].name);
  const opening=h.begin(ui.TARGETS[1].name);assert.equal(h.references.size,2);
  assert.ok(h.adapter.close().passed);h.setPermission(true);h.finish(opening);h.advance(500);
  assert.equal(h.calls.length,0);assert.equal(h.references.size,0);assert.equal(h.detached,1);
});

test('a nested GUI opening is queued rather than recursively executing more UI actions',()=>{
  let h;h=harness({onAction(controller){if(controller.class.fullName===ui.TARGETS[0].name)h.open(ui.TARGETS[1].name);}});
  h.open(ui.TARGETS[0].name);assert.equal(h.calls.length,1);assert.equal(h.references.size,1);
  h.advance(250);assert.equal(h.calls.length,2);assert.equal(h.references.size,0);assert.ok(h.adapter.close().passed);
});

test('a failed detach or reference release cannot be reported as successful cleanup',()=>{
  for(const options of [{detachFailure:true},{freeFailure:true}]) {
    const h=harness(options);h.setPermission(false);h.open(ui.TARGETS[0].name);
    const result=h.adapter.close();assert.equal(result.passed,false);assert.ok(result.errors.length>0);
  }
});

test('duplicate completion callbacks on an already-open controller cannot start a second action',()=>{
  const h=harness();const controller=h.open(ui.TARGETS[4].name);
  assert.equal(controller.state,2);h.completeAgain(controller);assert.equal(h.calls.length,1);
  assert.equal(h.references.size,0);assert.ok(h.adapter.close().passed);
});

test('a pooled controller can open again without retaining identities or references indefinitely',()=>{
  const h=harness();const controller=h.open(ui.TARGETS[0].name);
  for(let i=0;i<1000;i++) {h.reopen(controller);assert.equal(h.references.size,0);}
  assert.equal(h.calls.length,1001);assert.equal(h.adapter.snapshot().pending,0);
  assert.ok(h.adapter.close().passed);
});
