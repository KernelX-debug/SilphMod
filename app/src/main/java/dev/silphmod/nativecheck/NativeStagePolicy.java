package dev.silphmod.nativecheck;

final class NativeStagePolicy {
    static boolean directory(String value){return value!=null&&value.matches("/data/app/[A-Za-z0-9_./+=~-]+/lib/arm64")&&!value.contains("/../")&&!value.contains("/./");}
    static String observer(int mode,String hash){
        if((mode!=1&&mode!=2)||hash==null||!hash.matches("[a-f0-9]{64}"))throw new IllegalArgumentException("Invalid stage identity");
        return "libsilph-native-observer-"+mode+"-"+hash.substring(0,16)+".so";
    }
    static boolean namespaceRejected(String failure,String detail,boolean restored,boolean detached,boolean released,boolean restart,long actions){
        return "LINKER".equals(failure)&&detail!=null&&detail.contains("not accessible for the namespace")
            &&restored&&detached&&released&&!restart&&actions==0;
    }
    static int bootstrapRoute(int route,int attempt){
        if(route<0||route>3||attempt<0||attempt>7)throw new IllegalArgumentException("Invalid bootstrap route");
        return attempt%2==0?route:route^2;
    }
    static boolean retryBoundary(String failure,String stage,int error,boolean restored,boolean detached,boolean released,boolean restart,long actions,int returns){
        return "ATTACH".equals(failure)&&("syscall_boundary".equals(stage)||"legacy_syscall_boundary".equals(stage))&&(error==110||error==16)
            &&restored&&detached&&released&&!restart&&actions==0&&returns==0;
    }
    static String startupFailure(String failure,String stage,int signal,int code){
        if(signal>0)return "Native startup fault: signal "+signal+"/code "+code+" at "+stage+"; export logs";
        if("PROTOCOL".equals(failure))return "Native profile rejected; export logs";
        if("INITIALIZATION".equals(failure))return "Native initialization failed; export logs";
        return null;
    }
}
