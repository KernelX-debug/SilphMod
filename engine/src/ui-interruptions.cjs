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
  {name:'Niantic.Holoholo.Map.SpeedModal',kind:'speed_warning',action:'Dismiss'},
  {name:'Niantic.Holoholo.UI.NewStampCollectionGuiController',kind:'stamp_rally_notice',action:'Dismiss'},
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
  const acted=new Map();
  const unknown=new Map();
  let listener=null,visibleListener=null,closed=false,processing=false,nextDrain=0,actions=0,failures=0,expired=0;
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
  function observeUnknown(controller,source) {
    const name=controller.class.fullName,now=runtime.now();
    if(typeof name!=='string'||name.length>256||targets.has(name)||now-(unknown.get(name)??-Infinity)<30000)return;
    if(unknown.size>=32&&!unknown.has(name))unknown.delete(unknown.keys().next().value);
    unknown.set(name,now);
    report('ui_unknown_controller',{controller:name,source,action:'OBSERVE_ONLY'});
  }
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
          if(target.skip) {
            const asset=controller.field(target.skip.field).value;
            if(asset&&!asset.isNull()&&asset.class.fullName===target.skip.type&&live(asset)) {
              target.skip.method.bind(asset).invoke();
              report('ui_interruption_animation_skip',{kind:target.kind,controller:target.name,action:'SkipEggHatchAnimation'});
              if(closed)break;
            }
          }
          acted.set(entry.key,target.name);
          if(acted.size>256)acted.delete(acted.keys().next().value);
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
        let skip=null;
        if(runtime.visibleRecovery&&spec.kind==='egg_cutscene')try {
          const field=type.tryField('eggHatchAsset'),asset=runtime.resolve('PokemonInfoEggHatch');
          if(!field||field.isStatic||field.offset<16||field.type.name!=='PokemonInfoEggHatch')throw Error('Egg hatch asset unavailable');
          const cached=asset.tryField('m_CachedPtr');
          if(!cached||cached.isStatic||cached.type.name!=='System.IntPtr')throw Error('Egg hatch asset Unity pointer unavailable');
          skip={field:'eggHatchAsset',type:'PokemonInfoEggHatch',method:method(asset,'SkipEggHatchAnimation','System.Void')};
        }catch(error){report('ui_interruption_unavailable',{kind:'egg_animation_skip',message:String(error)});}
        targets.set(spec.name,{...spec,method:action,skip,disabled:false});
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
          if(!target){observeUnknown(controller,'OPEN');return;}
          if(!target||target.disabled||!live(controller)||stateOf(controller)!==1)return;
          const key=controller.handle.toString();
          acted.delete(key);
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
    if(listener&&runtime.visibleRecovery)visibleListener=runtime.attach(isTop.virtualAddress,{
      onEnter(args) {
        this.silphVisible=null;
        if(closed||processing||!runtime.canObserve()||!runtime.sameThread())return;
        try {
          const controller=runtime.object(args[0]),target=targets.get(controller.class.fullName);
          if(!target){observeUnknown(controller,'TOP_QUERY');return;}
          if(target&&!target.disabled)this.silphVisible=args[0];
        }catch(error){report('ui_interruption_deferred',{reason:'visible_observation_failed',message:String(error)});}
      },
      onLeave(result) {
        const address=this.silphVisible;this.silphVisible=null;
        if(!address||closed||processing||!runtime.canObserve()||!runtime.sameThread()||result.toInt32()===0)return;
        let target;
        try {
          const controller=runtime.object(address),key=controller.handle.toString();
          target=targets.get(controller.class.fullName);
          if(!target||target.disabled||queue.has(key)||acted.get(key)===target.name||entries.size>=MAX_PENDING
            ||!live(controller)||stateOf(controller)!==2)return;
          const entry={key,target,reference:runtime.retain(controller,'ui:visible:'+target.kind),created:runtime.now(),released:false};
          entries.add(entry);queue.set(key,entry);
          report('ui_interruption_seen',{kind:target.kind,controller:target.name,action:target.action,source:'VISIBLE_RECOVERY'});
        }catch(error){if(target)disable(target,'visible_recovery',error);}
      },
    });
  }catch(error){report('ui_interruption_unavailable',{controller:GUI_CLASS,message:String(error)});}
  report('ui_interruption_profile',{available:!!listener,visibleRecovery:!!visibleListener,eventDriven:true,originalOpenPreserved:true,
    targets:[...targets.values()].map(({name,kind,action})=>({name,kind,action})),maxPending:MAX_PENDING});
  function snapshot() {
    return {available:!!listener&&!closed,pending:queue.size,references:entries.size,actions,failures,expired,
      enabledTargets:[...targets.values()].filter(target=>!target.disabled).length};
  }
  function close() {
    closed=true;
    if(listener)try {listener.detach();listener=null;}catch(error){errors.push(String(error));}
    if(visibleListener)try {visibleListener.detach();visibleListener=null;}catch(error){errors.push(String(error));}
    acted.clear();unknown.clear();
    for(const entry of [...entries])release(entry);
    return {passed:errors.length===0&&listener===null&&visibleListener===null&&entries.size===0,remainingHooks:(listener?1:0)+(visibleListener?1:0),
      remainingReferences:entries.size,errors:[...errors],actions,failures,expired};
  }
  return {drain,snapshot,close,get busy(){return processing;}};
}

module.exports={GUI_CLASS,OPEN_COMPLETED,TARGETS,MAX_PENDING,MAX_ACTIONS,RETRY_MS,EXPIRY_MS,install};
