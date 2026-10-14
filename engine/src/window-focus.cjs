'use strict';
const {jniRuntime}=require('../location/jni-runtime.cjs');

function installWindowFocus(runtime,report) {
  const jni=jniRuntime(runtime,(event,detail)=>{
    if(event!=='jni_scope_released'||!detail.passed)report('window_focus_jni',{traceEvent:event,...detail});
  });
  let playerClass=null,activityField=null,focusMethod=null,closed=false;
  let lastError='',nextError=0;
  function close() {
    closed=true;const errors=[];
    if(playerClass)try {
      jni.withEnv(scope=>{scope.release(playerClass);playerClass=null;});
    }catch(error){errors.push(String(error));}
    return {passed:errors.length===0,remainingClassRefs:playerClass?1:0,errors};
  }
  try {
    jni.withEnv(scope=>{
      const player=scope.loadApplicationClass('com.unity3d.player.UnityPlayer');
      activityField=scope.staticField(player,'currentActivity','Landroid/app/Activity;');
      focusMethod=scope.method(scope.findClass('android/app/Activity'),'hasWindowFocus','()Z');
      playerClass=scope.retain(player);
    });
  }catch(error){
    const rollback=close();report('window_focus_setup_failed',{message:String(error),rollback});
    const failed=new Error(String(error));failed.windowFocusRollback=rollback;throw failed;
  }
  report('window_focus_profile',{backend:'JNI_PUBLIC_ABI',javaBridgeLoaded:false,
    hooksInstalled:0,activityRetained:false,coordinateMutation:false,systemMutation:false});
  return {
    focused() {
      if(closed)return false;
      try {
        const focused=jni.withEnv(scope=>{
          const activity=scope.staticObject(playerClass,activityField);
          if(!activity||activity.isNull())return false;
          return scope.scalar(activity,focusMethod,'boolean')!==0;
        });
        lastError='';return focused;
      }catch(error){
        const message=String(error),now=Date.now();
        if(message!==lastError||now>=nextError){report('window_focus_read_failed',{message,actionsAllowed:false});nextError=now+30000;}
        lastError=message;return false;
      }
    },close,
  };
}
module.exports={installWindowFocus};
