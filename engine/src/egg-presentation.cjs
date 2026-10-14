'use strict';
const MAP='Niantic.Holoholo.Map.MapExploreState',DATA='Niantic.Holoholo.EggHatch.HatchedPokemonsData';
function install(runtime,report) {
  let restore=null,closed=false,busy=false,skipped=0;
  const scope=runtime.scope??'ACTIVE_ENGINE';
  const errors=[];
  try {
    const type=runtime.resolve(MAP);
    const matches=type.methods.filter(method=>!method.isStatic&&method.returnType.name==='System.Void'
      &&method.parameters.length===1&&method.parameters[0].type.name===DATA&&!method.virtualAddress.isNull());
    if(matches.length!==1)throw Error('Egg presentation signature is not unique: '+matches.length);
    const method=matches[0];
    restore=runtime.replace(method,()=>{
      try {if(closed||busy||!runtime.sameThread()||!runtime.canSkip())return false;}
      catch(error){report('ui_interruption_deferred',{kind:'egg_hatch',reason:'presentation_guard_failed',message:String(error)});return false;}
      busy=true;
      try {skipped++;report('ui_interruption_presentation_skipped',{kind:'egg_hatch',controller:MAP,method:method.name,
        stage:'BEFORE_SCENE',scope,touchesSent:0});return true;}
      finally {busy=false;}
    });
    report('egg_presentation_profile',{available:true,controller:MAP,method:method.name,parameter:DATA,scope});
  }catch(error){report('egg_presentation_profile',{available:false,message:String(error),scope});}
  return {get busy(){return busy;},close(){closed=true;if(restore)try {restore();restore=null;}catch(error){errors.push(String(error));}
    return {passed:errors.length===0&&!restore,errors,skipped};}};
}
module.exports={install,MAP,DATA};
