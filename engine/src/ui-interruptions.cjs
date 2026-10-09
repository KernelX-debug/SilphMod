'use strict';

const GUI_CLASS='Niantic.Holoholo.NUI.GuiController';
const OPEN_COMPLETED='<Open>b__84_2';
const TARGETS=Object.freeze([
  {name:'Niantic.Holoholo.UI.BadgeIncrementGuiController',kind:'medal',action:'Dismiss'},
  {name:'Items.PlayerBuffs.Incense.DailyIncenseRecapGuiController',kind:'daily_incense_recap',action:'Dismiss'},
  {name:'Niantic.Holoholo.UI.LevelUpV3GuiController',kind:'trainer_level',action:'Dismiss'},
  {name:'Niantic.Holoholo.UI.XpAwardGuiControllerV2',kind:'xp_award',action:'LuckyBonusSkipButtonClicked'},
  {name:'Niantic.Holoholo.UI.EggHatchGuiController',kind:'egg_intro',action:'Click'},
  {name:'Niantic.Holoholo.Cutscenes.EggHatchCutsceneGuiController',kind:'egg_cutscene',action:'Dismiss'},
].map(target=>Object.freeze(target)));
const MAX_PENDING=16, MAX_ACTIONS=2, RETRY_MS=250, EXPIRY_MS=30000;

function method(type,name,result) {
  const value=type.tryMethod(name,0);
  if(!value||value.isStatic||value.parameterCount!==0||value.returnType.name!==result||value.virtualAddress.isNull())
    throw Error('Incompatible GUI method: '+type.fullName+'.'+name);
  return value;
}

function install(runtime,report) {
  const targets=new Map(),entries=new Set(),queue=new Map(),errors=[];
  let listener=null,closed=false,processing=false,nextDrain=0,actions=0,failures=0,expired=0;
  let base,opened,isTop,state;
  function release(entry) {
    if(entry.released)return;
    entry.released=true;queue.delete(entry.key);
    try {entry.reference.free();entries.delete(entry);}
    catch(error){errors.push(String(error));report('ui_interruption_error',{kind:entry.target.kind,phase:'release',message:String(error)});}
  }
  function disable(target,phase,error) {
    target.disabled=true;failures++;
    report('ui_interruption_error',{kind:target.kind,controller:target.name,phase,message:String(error),farmingContinues:true});
    for(const entry of [...entries])if(entry.target===target)release(entry);
  }
  function stateOf(controller) {return runtime.number(controller.field(state.name).value);}
  function live(controller) {
    const value=controller.field('m_CachedPtr').value;
    if(!runtime.isPointer(value))throw Error('Incompatible Unity object pointer');
    return !value.isNull();
  }
  function drain(force=false) {
    if(closed||processing||queue.size===0||!runtime.sameThread())return;
    const now=runtime.now();
    if(!force&&now<nextDrain)return;
    nextDrain=now+RETRY_MS;
    for(const entry of [...queue.values()])if(now-entry.created>=EXPIRY_MS) {
      expired++;report('ui_interruption_deferred',{kind:entry.target.kind,reason:'expired_without_permission',ageMs:now-entry.created});release(entry);
    }
    if(queue.size===0||!runtime.canObserve())return;
    processing=true;
    try {
      if(!runtime.canAct())return;
      let attempted=0;
      for(const entry of [...queue.values()]) {
        if(attempted>=MAX_ACTIONS||closed)break;
        const target=entry.target;
        try {
          const controller=runtime.read(entry.reference);
          if(controller.class.fullName!==target.name)throw Error('GUI identity changed');
          if(!live(controller)||stateOf(controller)!==2){release(entry);continue;}
          if(!isTop.bind(controller).invoke())continue;
          if(!runtime.canAct())break;
          attempted++;const began=runtime.now();
          // The game's action keeps its dismissal events, promises and cleanup.
          target.method.bind(controller).invoke();actions++;
          const after=stateOf(controller);
          report('ui_interruption_result',{kind:target.kind,controller:target.name,action:target.action,
            stateAfter:after,dismissStarted:after===3,fullyDismissed:after===0,elapsedMs:runtime.now()-began});
          release(entry);
        }catch(error){disable(target,'action',error);}
      }
    }catch(error){report('ui_interruption_deferred',{reason:'session_not_ready',message:String(error)});}
    finally {processing=false;}
  }
  try {
    base=runtime.resolve(GUI_CLASS);
    opened=method(base,OPEN_COMPLETED,'System.Void');
    isTop=method(base,'get_IsTopGui','System.Boolean');
    state=base.tryField('currentState');
    if(!state||state.isStatic||state.offset<16||!state.type.class.isEnum||state.type.class.baseType?.name!=='System.Int32')
      throw Error('Incompatible GUI open state');
    for(const spec of TARGETS) {
      try {
        const type=runtime.resolve(spec.name);
        if(![...type.hierarchy()].some(parent=>parent.fullName===GUI_CLASS))throw Error('Not a GuiController');
        const action=method(type,spec.action,'System.Void');
        const cached=type.tryField('m_CachedPtr');
        if(!cached||cached.isStatic||cached.type.name!=='System.IntPtr')throw Error('Unity object field unavailable');
        targets.set(spec.name,{...spec,method:action,disabled:false});
      }catch(error){report('ui_interruption_unavailable',{...spec,message:String(error)});}
    }
    if(targets.size)listener=runtime.attach(opened.virtualAddress,{
      onEnter(args) {
        this.silphEntry=null;
        if(closed||!runtime.canObserve()||!runtime.sameThread())return;
        let target;
        try {
          const controller=runtime.object(args[0]);
          target=targets.get(controller.class.fullName);
          if(!target||target.disabled||!live(controller)||stateOf(controller)!==1)return;
          const key=controller.handle.toString();
          if(queue.has(key)||entries.size>=MAX_PENDING)return;
          const reference=runtime.retain(controller,'ui:'+target.kind);
          const entry={key,target,reference,created:runtime.now(),released:false};
          entries.add(entry);this.silphEntry=entry;
        }catch(error){if(target)disable(target,'observe',error);}
      },
      onLeave() {
        const entry=this.silphEntry;this.silphEntry=null;
        if(!entry||entry.released)return;
        if(closed){release(entry);return;}
        queue.set(entry.key,entry);
        report('ui_interruption_seen',{kind:entry.target.kind,controller:entry.target.name,action:entry.target.action});
        drain(true);
      },
    });
  }catch(error){report('ui_interruption_unavailable',{controller:GUI_CLASS,message:String(error)});}
  report('ui_interruption_profile',{available:!!listener,eventDriven:true,originalOpenPreserved:true,
    targets:[...targets.values()].map(({name,kind,action})=>({name,kind,action})),maxPending:MAX_PENDING});
  function snapshot() {
    return {available:!!listener&&!closed,pending:queue.size,references:entries.size,actions,failures,expired,
      enabledTargets:[...targets.values()].filter(target=>!target.disabled).length};
  }
  function close() {
    closed=true;
    if(listener)try {listener.detach();listener=null;}catch(error){errors.push(String(error));}
    for(const entry of [...entries])release(entry);
    return {passed:errors.length===0&&listener===null&&entries.size===0,remainingHooks:listener?1:0,
      remainingReferences:entries.size,errors:[...errors],actions,failures,expired};
  }
  return {drain,snapshot,close,get busy(){return processing;}};
}

module.exports={GUI_CLASS,OPEN_COMPLETED,TARGETS,MAX_PENDING,MAX_ACTIONS,RETRY_MS,EXPIRY_MS,install};
