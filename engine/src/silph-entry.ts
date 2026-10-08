// @ts-ignore Own tested bootstrap controller.
import bootstrap from './silph-bootstrap.cjs';
// @ts-ignore JNI adapter validated for the supported UGPhone environment.
import trace from '../location/location-trace.cjs';

const api=bootstrap.createBootstrap({Process,Module,Memory,NativeFunction,Interceptor,ptr,
  librariesReady:()=>Process.findModuleByName('libil2cpp.so')!==null&&Process.findModuleByName('libunity.so')!==null},
  trace.installTrace,async()=>{
    // esbuild keeps the bridge imports inside this deferred module initializer.
    const loaded=await import('./agent.js');
    rpc.exports=api;
    return loaded.enginePort;
  },(event:string,detail:Record<string,unknown>={})=>{
    send(['frida:stdout',JSON.stringify({type:'send',payload:{protocol:1,event,time:Date.now(),...detail}})+'\n']);
  },()=>Date.now(),{setImmediate,setInterval,clearInterval});
rpc.exports=api;
