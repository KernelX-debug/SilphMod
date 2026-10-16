'use strict';

const HUD='Niantic.Holoholo.Map.UI.PlayerHudGuiController';
const PROFILE='Niantic.Holoholo.UI.PlayerProfilePage';
const FRIENDS='Niantic.Holoholo.Social.UI.FriendsListHeaderCellView';
const GIFT='Niantic.Holoholo.Social.Gifting.UI.OpenGiftGuiController';
const STAGE='Niantic.Holoholo.Social.Gifting.UI.GiftBoxStage';
const REWARDS='Niantic.Holoholo.UI.MajorRewardSequenceView';
const WAIT='Niantic.Zeppelin.Scheduler.Wait';
const LABEL='SilphMod',MAX_RECORDS=48,ANIMATION_SPEED=12,OPEN_SECONDS=0.05;

function method(type,name,parameters,result) {
  const value=type.tryMethod(name,parameters.length);
  if(!value||value.isStatic||value.virtualAddress.isNull()||value.returnType.name!==result
    ||value.parameters.length!==parameters.length||value.parameters.some((p,i)=>p.type.name!==parameters[i]))
    throw Error('Incompatible game tool method: '+type.fullName+'.'+name);
  return value;
}
function field(type,name,expected) {
  const value=type.tryField(name);
  if(!value||value.isStatic||value.offset<16||value.type.name!==expected)
    throw Error('Incompatible game tool field: '+type.fullName+'.'+name);
  return value;
}
function install(runtime,options,report) {
  const hooks=[],records=new Map(),errors=[];
  let closed=false,names=false,gifts=false,operations=0,giftDepth=0;
  let labelReference=null,nameUpdates=0,giftOpens=0,animations=0,giftWaits=0,savedWaitSeconds=0,failures=0;
  let hudPrimed=false;
  function enabled(feature){return !closed&&(feature==='names'?names:gifts)&&runtime.enabled()&&runtime.sameThread();}
  function error(feature,phase,cause){
    if(feature==='names')names=false;else gifts=false;
    failures++;report('game_tools_error',{feature,phase,message:String(cause),farmingContinues:true});
  }
  function object(address,type){
    const value=runtime.object(address);
    if(!runtime.valid(value)||value.class.fullName!==type)throw Error('Invalid game tool controller: '+type);
    return value;
  }
  function remember(value,kind,read,write,replacement,group) {
    if(!runtime.valid(value)||!runtime.alive(value))return;
    const key=kind+':'+value.handle.toString();
    let entry=records.get(key);
    const current=read(value);
    if(current===replacement)return;
    if(!entry){
      for(const [oldKey,old] of [...records]){
        const held=runtime.object(runtime.address(old.reference));
        if(runtime.valid(held)&&runtime.alive(held))continue;
        old.reference.free();records.delete(oldKey);
      }
      if(records.size>=MAX_RECORDS)throw Error('Game tool reference limit reached');
      entry={reference:runtime.retain(value,'game-tools:'+kind),kind,read,write,original:current,replacement,group};
      records.set(key,entry);
    }else if(current!==entry.replacement)entry.original=current;
    write(value,replacement);
  }
  function release(group,restore=true){
    for(const [key,entry] of [...records]){
      if(group&&entry.group!==group)continue;
      try{
        const value=runtime.object(runtime.address(entry.reference));
        if(restore&&runtime.valid(value)&&runtime.alive(value)&&entry.read(value)===entry.replacement)
          entry.write(value,entry.original);
      }catch(cause){errors.push(String(cause));}
      try{entry.reference.free();records.delete(key);}catch(cause){errors.push(String(cause));}
    }
  }
  function listen(value,callbacks){const listener=runtime.attach(value.virtualAddress,callbacks);hooks.push(listener);}
  function run(feature,phase,work){
    if(!enabled(feature))return;
    operations++;
    try{work();}catch(cause){error(feature,phase,cause);}finally{operations--;}
  }
  function setName(controller,fields){
    if(!labelReference)labelReference=runtime.retain(runtime.string(LABEL),'game-tools:label');
    for(const name of fields){
      const text=controller.field(name).value;
      remember(text,'text',value=>runtime.stringContent(value.method('get_text',0).invoke()),
        (value,content)=>value.method('set_text',1).invoke(content===LABEL?runtime.stringObject(labelReference):runtime.string(content)),LABEL,'names');
      nameUpdates++;
    }
  }
  function speed(animator){
    if(!runtime.valid(animator))return;
    remember(animator,'speed',value=>{
      const speed=value.method('get_speed',0).invoke();
      if(!Number.isFinite(speed)||speed<0||speed>100)throw Error('Invalid gift animation speed');
      return speed;
    },(value,speed)=>value.method('set_speed',1).invoke(speed),ANIMATION_SPEED,'gifts');
    animations++;
  }
  if(options.hideTrainerName)try{
    const text=runtime.resolve('UnityEngine.UI','UnityEngine.UI.Text');
    method(text,'get_text',[],'System.String');method(text,'set_text',['System.String'],'System.Void');
    const profiles=[
      {type:HUD,fields:['playerName','playerNameShadow'],methods:[['Initialize',[]],['SetVisible',['System.Boolean']],['coiw',['Niantic.Holoholo.PlayerService.PlayerInfo']]]},
      {type:PROFILE,fields:['playerNameText'],methods:[['Reinitialize',[]]]},
      {type:FRIENDS,fields:['playerName'],methods:[['SetI18NText',[]]]},
    ];
    const resolved=profiles.map(profile=>{
      const type=runtime.resolve('holo-game',profile.type);
      profile.fields.forEach(name=>field(type,name,'UnityEngine.UI.Text'));
      return {...profile,methods:profile.methods.map(([name,parameters])=>method(type,name,parameters,'System.Void'))};
    });
    names=true;
    for(const profile of resolved)for(const value of profile.methods)listen(value,{
      onEnter(args){this.silphNameController=enabled('names')?args[0]:null;},
      onLeave(){if(this.silphNameController)run('names','display',()=>setName(object(this.silphNameController,profile.type),profile.fields));},
    });
  }catch(cause){error('names','profile',cause);}
  if(options.fastGiftOpening)try{
    const gift=runtime.resolve('holo-game',GIFT),stage=runtime.resolve('holo-game',STAGE),reward=runtime.resolve('holo-game',REWARDS);
    const animator=runtime.resolve('UnityEngine.AnimationModule','UnityEngine.Animator');
    method(animator,'get_speed',[],'System.Single');method(animator,'set_speed',['System.Single'],'System.Void');
    field(gift,'giftOpenAnimationLength','System.Single');field(gift,'guiAnimator','UnityEngine.Animator');
    field(stage,'dloi','GiftBox');field(runtime.resolve('holo-game','GiftBox'),'giftBoxAnimator','UnityEngine.Animator');
    field(reward,'animator','UnityEngine.Animator');
    const click=method(gift,'OnOpenClick',[],'System.Void'),dismiss=method(gift,'HandleDismissComplete',[],'System.Void');
    const stageOpen=method(stage,'AnimateGiftBoxOpen',[],'System.Void');
    const rewardInit=method(reward,'Initialize',['Niantic.Holoholo.UI.MajorRewardSequenceView.MajorRewardSequenceData'],'System.Void');
    field(runtime.resolve('Zeppelin',WAIT),'<SleepFor>k__BackingField','System.Single');
    const coroutines=[['ddh','dlrn'],['ddi','dlrq']].map(([name,current])=>{
      const type=gift.nested(name);field(type,current,'Niantic.Zeppelin.Scheduler.ISchedule');
      return {type,current,method:method(type,'MoveNext',[],'System.Boolean')};
    });
    gifts=true;
    listen(click,{onEnter(args){run('gifts','open',()=>{
      const controller=object(args[0],GIFT);
      remember(controller,'open-duration',value=>{
        const seconds=value.field('giftOpenAnimationLength').value;
        if(!Number.isFinite(seconds)||seconds<0||seconds>60)throw Error('Invalid gift animation duration');
        return seconds;
      },(value,seconds)=>{value.field('giftOpenAnimationLength').value=seconds;},OPEN_SECONDS,'gifts');
      speed(controller.field('guiAnimator').value);giftOpens++;
      report('gift_open_accelerated',{manual:true,animationSeconds:OPEN_SECONDS,animationSpeed:ANIMATION_SPEED,requestChanged:false,open:giftOpens});
    });}});
    for(const coroutine of coroutines)listen(coroutine.method,{
      onEnter(args){this.silphGiftScope=enabled('gifts');if(this.silphGiftScope){this.silphGiftIterator=args[0];giftDepth++;}},
      onLeave(result){
        if(!this.silphGiftScope)return;
        try{
          if(result.toInt32()===1)run('gifts','presentation-wait',()=>{
            const current=object(this.silphGiftIterator,coroutine.type.fullName).field(coroutine.current).value;
            if(!runtime.valid(current)||current.class.fullName!==WAIT)return;
            const duration=current.field('<SleepFor>k__BackingField');
            const seconds=duration.value;
            if(!Number.isFinite(seconds)||seconds<0||seconds>5)return;
            if(seconds>OPEN_SECONDS+0.001){duration.value=OPEN_SECONDS;giftWaits++;savedWaitSeconds+=seconds-OPEN_SECONDS;}
          });
        }finally{giftDepth--;}
      },
    });
    listen(stageOpen,{onEnter(args){run('gifts','box-animation',()=>{
      const box=object(args[0],STAGE).field('dloi').value;
      if(runtime.valid(box))speed(box.field('giftBoxAnimator').value);
    });}});
    listen(rewardInit,{
      onEnter(args){this.silphGiftReward=giftDepth>0&&enabled('gifts')?args[0]:null;},
      onLeave(){if(this.silphGiftReward)run('gifts','reward-animation',()=>speed(object(this.silphGiftReward,REWARDS).field('animator').value));},
    });
    listen(dismiss,{onLeave(){if(runtime.sameThread()){operations++;try{release('gifts');}finally{operations--;}}}});
  }catch(cause){error('gifts','profile',cause);}
  report('game_tools_profile',{hideTrainerName:names,fastGiftOpening:gifts,label:LABEL,visualOnly:true,
    animationSpeed:ANIMATION_SPEED,openAnimationSeconds:OPEN_SECONDS,scopedGiftWaits:true,networkWaitsChanged:false,referenceLimit:MAX_RECORDS});
  function primeHud(lookup=runtime.hud){
    if(hudPrimed||!enabled('names'))return;
    let hud;
    try{hud=lookup();if(!runtime.valid(hud)||!runtime.alive(hud))return;}
    catch(cause){hudPrimed=true;report('game_tools_hud_deferred',{message:String(cause),uiHooksAvailable:names});return;}
    hudPrimed=true;
    run('names','initial-hud',()=>setName(object(hud.handle,HUD),['playerName','playerNameShadow']));
    report('game_tools_hud_ready',{masked:names,label:LABEL});
  }
  function snapshot(){return {hideTrainerName:names&&!closed,fastGiftOpening:gifts&&!closed,nameUpdates,giftOpens,animations,giftWaits,
    savedWaitSeconds:Math.round(savedWaitSeconds*1000)/1000,
    references:records.size+(labelReference?1:0),giftReferences:[...records.values()].filter(entry=>entry.group==='gifts').length,
    activeOperations:operations,giftDepth,failures};}
  function close(){
    closed=true;
    for(const listener of [...hooks])try{listener.detach();hooks.splice(hooks.indexOf(listener),1);}catch(cause){errors.push(String(cause));}
    const restore=records.size===0||runtime.sameThread();
    if(!restore)errors.push('Game tool restoration requires the Unity thread');
    release(null,restore);
    if(labelReference)try{labelReference.free();labelReference=null;}catch(cause){errors.push(String(cause));}
    return {passed:errors.length===0&&hooks.length===0&&records.size===0&&!labelReference&&operations===0&&giftDepth===0,
      errors:[...errors],remainingHooks:hooks.length,remainingReferences:records.size+(labelReference?1:0),nameUpdates,giftOpens,animations,giftWaits,failures};
  }
  return {primeHud,snapshot,close,get busy(){return operations>0||giftDepth>0;},get needsUnityCleanup(){return records.size>0;}};
}
module.exports={install,HUD,PROFILE,FRIENDS,GIFT,STAGE,REWARDS,WAIT,LABEL,MAX_RECORDS,ANIMATION_SPEED,OPEN_SECONDS};
