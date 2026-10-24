#define _GNU_SOURCE
#include "location-tools.h"
#include "code-memory.h"
#include <jni.h>
#include <shadowhook.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>
typedef jint (*GetVMs)(JavaVM **,jsize,jsize *);
typedef jboolean (*CallV)(JNIEnv *,jobject,jmethodID,va_list);
typedef jboolean (*CallA)(JNIEnv *,jobject,jmethodID,const jvalue *);
typedef struct {void *address,*stub,*original;SilphCodeMemory code;unsigned code_dirty;} Hook;
typedef struct {
    atomic_uint active,inflight,state,error,stage;atomic_uint_fast64_t calls,changed,legacy,modern,exceptions,objects;
    JavaVM *vm;jclass location;jmethodID methods[2];Hook hooks[3];unsigned installed,method_bits,cleanup;
    SilphLocationAllowed allowed;void *context;unsigned code_index;
} Location;
static Location current;
static pthread_mutex_t lifecycle=PTHREAD_MUTEX_INITIALIZER;
static int mapped(const void *address,size_t bytes,char permission){
    uintptr_t p=(uintptr_t)address;if(permission=='r')p&=UINT64_C(0x00ffffffffffffff);if(!p||!bytes||p>UINTPTR_MAX-bytes)return 0;
    FILE *file=fopen("/proc/self/maps","re");if(!file)return 0;char line[1024];int found=0;
    for(unsigned i=0;i<32768&&fgets(line,sizeof(line),file);i++){uintptr_t lo=0,hi=0;char mode[5]={0};
        if(sscanf(line,"%" SCNxPTR "-%" SCNxPTR " %4s",&lo,&hi,mode)==3&&p>=lo&&p+bytes<=hi&&strchr(mode,permission)){found=1;break;}}
    fclose(file);return found;
}
static int env_valid(JNIEnv *env){
    if(!mapped(env,sizeof(*env),'r')||!mapped(*env,offsetof(struct JNINativeInterface,GetObjectRefType)+sizeof(void *),'r'))return 0;
#define VALID(member) if(!mapped((const void *)(*env)->member,4,'x'))return 0
    VALID(ExceptionCheck);VALID(ExceptionClear);VALID(FindClass);VALID(GetMethodID);VALID(NewGlobalRef);VALID(DeleteGlobalRef);VALID(DeleteLocalRef);VALID(IsInstanceOf);
    VALID(CallBooleanMethod);VALID(CallBooleanMethodV);VALID(CallBooleanMethodA);
#undef VALID
    return 1;
}
static int get_env(JavaVM *vm,JNIEnv **out,bool *owned){
    *out=NULL;*owned=false;
    if(!mapped(vm,sizeof(*vm),'r'))return 31;if(!mapped(*vm,sizeof(struct JNIInvokeInterface),'r'))return 32;
    if(!mapped((const void *)(*vm)->GetEnv,4,'x')||!mapped((const void *)(*vm)->AttachCurrentThreadAsDaemon,4,'x')||!mapped((const void *)(*vm)->DetachCurrentThread,4,'x'))return 33;
    jint result=(*vm)->GetEnv(vm,(void **)out,JNI_VERSION_1_6);
    if(result==JNI_EDETACHED){result=(*vm)->AttachCurrentThreadAsDaemon(vm,out,NULL);if(result==JNI_OK)*owned=true;else return 35;}
    if(result!=JNI_OK)return 34;if(!env_valid(*out))return 36;return 0;
}
static int detach(JavaVM *vm,bool owned){return owned&&(*vm)->DetachCurrentThread(vm)!=JNI_OK;}
static jboolean filter(JNIEnv *env,jobject object,jmethodID method,jboolean result,bool pending){
    if(!atomic_load(&current.active))return result;
    unsigned kind=method==current.methods[0]?1:current.methods[1]&&method==current.methods[1]?2:0;if(!kind||!current.allowed(current.context))return result;
    if(pending||(*env)->ExceptionCheck(env)){atomic_fetch_add(&current.exceptions,1);return result;}
    if(!object||!(*env)->IsInstanceOf(env,object,current.location)){atomic_fetch_add(&current.objects,1);return result;}
    if((*env)->ExceptionCheck(env)){atomic_fetch_add(&current.exceptions,1);return result;}
    atomic_fetch_add(&current.calls,1);atomic_fetch_add(kind==1?&current.legacy:&current.modern,1);if(result)atomic_fetch_add(&current.changed,1);return JNI_FALSE;
}
static bool pending(JNIEnv *env,jmethodID method){return atomic_load(&current.active)&&(method==current.methods[0]||(current.methods[1]&&method==current.methods[1]))&&(*env)->ExceptionCheck(env);}
static jboolean call_v(JNIEnv *env,jobject object,jmethodID method,va_list args){
    atomic_fetch_add(&current.inflight,1);bool before=pending(env,method);CallV original=NULL;memcpy(&original,&current.hooks[1].original,sizeof(original));
    jboolean result=original(env,object,method,args);result=filter(env,object,method,result,before);atomic_fetch_sub(&current.inflight,1);return result;
}
static jboolean call_a(JNIEnv *env,jobject object,jmethodID method,const jvalue *args){
    atomic_fetch_add(&current.inflight,1);bool before=pending(env,method);CallA original=NULL;memcpy(&original,&current.hooks[2].original,sizeof(original));
    jboolean result=original(env,object,method,args);result=filter(env,object,method,result,before);atomic_fetch_sub(&current.inflight,1);return result;
}
static jboolean call_variadic(JNIEnv *env,jobject object,jmethodID method,...){
    atomic_fetch_add(&current.inflight,1);bool before=pending(env,method);va_list args;va_start(args,method);CallV original=NULL;memcpy(&original,&current.hooks[1].original,sizeof(original));
    jboolean result=original(env,object,method,args);va_end(args);result=filter(env,object,method,result,before);atomic_fetch_sub(&current.inflight,1);return result;
}
static int drained(void){
    for(unsigned i=0;i<100;i++){if(!atomic_load(&current.inflight))return 0;struct timespec pause={0,1000000};nanosleep(&pause,NULL);}return 1;
}
void silph_location_metrics(SilphLocationMetrics *out){
    if(!out)return;pthread_mutex_lock(&lifecycle);unsigned remaining=0;for(unsigned i=0;i<3;i++)if(current.hooks[i].stub)remaining++;
    SilphCodeMemory *code=&current.hooks[current.code_index?current.code_index-1:0].code;
    *out=(SilphLocationMetrics){.state=atomic_load(&current.state),.error=atomic_load(&current.error),.stage=atomic_load(&current.stage),.methods=current.method_bits,
        .read_method=code->read_method,.read_errno=code->read_errno,.mem_errno=code->mem_errno,.protect_errno=code->protect_errno,.restore_errno=code->restore_errno,
        .hook_index=current.code_index,.code_protection=code->count?(uint32_t)code->pages[0].protection:0,
        .hooks=current.installed,.remaining=remaining,.global_refs=current.location?1u:0u,.cleanup=current.cleanup,.inflight=atomic_load(&current.inflight),.calls=atomic_load(&current.calls),.changed=atomic_load(&current.changed),
        .legacy_calls=atomic_load(&current.legacy),.modern_calls=atomic_load(&current.modern),.exception_bypass=atomic_load(&current.exceptions),.object_bypass=atomic_load(&current.objects)};pthread_mutex_unlock(&lifecycle);
}
static int close_location(void){
    atomic_store(&current.active,0);if(drained())return 20;int status=0;
    const unsigned order[]={0,2,1};for(unsigned n=0;n<3;n++){Hook *h=&current.hooks[order[n]];
        if(h->stub){h->code_dirty=1;int readable=silph_code_readable(&h->code),unhooked=readable?-1:shadowhook_unhook(h->stub);
            if(!unhooked)h->stub=NULL;if(readable||unhooked||silph_code_matches(&h->code))status=21;}
        if(h->code_dirty){if(silph_code_restore(&h->code))status=21;else h->code_dirty=0;}}
    if(status||drained())return status?status:22;
    if(current.location){JNIEnv *env=NULL;bool owned=false;if(get_env(current.vm,&env,&owned)){detach(current.vm,owned);return 23;}
        if((*env)->ExceptionCheck(env)){detach(current.vm,owned);return 24;}(*env)->DeleteGlobalRef(env,current.location);current.location=NULL;if(detach(current.vm,owned))return 25;}
    current.cleanup=1;atomic_store(&current.state,2);return 0;
}
static int open_location(void *get_vms,SilphLocationAllowed allowed,void *context){
    if(!get_vms||!allowed||atomic_load(&current.active)||atomic_load(&current.inflight))return 1;
    for(unsigned i=0;i<3;i++)if(current.hooks[i].stub||current.hooks[i].code_dirty)return 1;memset(&current,0,sizeof(current));current.allowed=allowed;current.context=context;
    atomic_store(&current.stage,1);int status=0;GetVMs get=NULL;memcpy(&get,&get_vms,sizeof(get));JNIEnv *env=NULL;bool owned=false;jclass local=NULL;jsize count=0;
    if(!mapped(get_vms,4,'x')||get(&current.vm,1,&count)!=JNI_OK||count!=1||!current.vm){status=2;goto done;}
    atomic_store(&current.stage,2);status=get_env(current.vm,&env,&owned);if(status)goto done;
    if((*env)->ExceptionCheck(env)){status=4;goto done;}
    atomic_store(&current.stage,3);local=(*env)->FindClass(env,"android/location/Location");if((*env)->ExceptionCheck(env)){(*env)->ExceptionClear(env);status=5;goto done;}if(!local){status=5;goto done;}
    current.location=(*env)->NewGlobalRef(env,local);if((*env)->ExceptionCheck(env)){(*env)->ExceptionClear(env);status=6;goto done;}if(!current.location){status=6;goto done;}
    const char *names[]={"isFromMockProvider","isMock"};for(unsigned i=0;i<2;i++){
        current.methods[i]=(*env)->GetMethodID(env,local,names[i],"()Z");if((*env)->ExceptionCheck(env)){(*env)->ExceptionClear(env);current.methods[i]=NULL;}
        if(current.methods[i])current.method_bits|=1u<<i;}
    if(!current.methods[0]||(current.methods[1]&&current.methods[0]==current.methods[1])){status=7;goto done;}
    atomic_store(&current.stage,4);current.hooks[0].address=(void *)(*env)->CallBooleanMethod;current.hooks[1].address=(void *)(*env)->CallBooleanMethodV;current.hooks[2].address=(void *)(*env)->CallBooleanMethodA;
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<i;j++)if(current.hooks[i].address==current.hooks[j].address){status=8;goto done;}
    for(unsigned i=0;i<3;i++){
        current.code_index=i+1;
        if(!mapped(current.hooks[i].address,16,'x')||((uintptr_t)current.hooks[i].address&3)){status=9;goto done;}
        if(silph_code_snapshot(current.hooks[i].address,&current.hooks[i].code)){
            current.hooks[i].code_dirty=!!current.hooks[i].code.dirty_mask;status=12;goto done;}}
    atomic_store(&current.stage,5);void *proxies[]={(void *)call_variadic,(void *)call_v,(void *)call_a};const unsigned order[]={1,2,0};
    for(unsigned n=0;n<3;n++){unsigned i=order[n];Hook *h=&current.hooks[i];
        current.code_index=i+1;
        h->code_dirty=1;int readable=silph_code_readable(&h->code);
        if(!readable)h->stub=shadowhook_hook_func_addr(h->address,proxies[i],&h->original);
        int hook_error=shadowhook_get_errno(),restored=silph_code_restore(&h->code);if(!restored)h->code_dirty=0;
        if(readable||restored){status=13;goto done;}if(!h->stub||!h->original){status=10+hook_error;goto done;}current.installed++;}
done:
    if(local)(*env)->DeleteLocalRef(env,local);if(current.vm&&detach(current.vm,owned)&&!status)status=11;
    if(status){unsigned phase=atomic_load(&current.stage);int cleanup=close_location();atomic_store(&current.stage,phase);atomic_store(&current.error,status);atomic_store(&current.state,3);return cleanup?100+cleanup:status;}
    atomic_store(&current.stage,6);atomic_store(&current.state,1);atomic_store(&current.active,1);return 0;
}
int silph_location_open(void *get_vms,SilphLocationAllowed allowed,void *context){pthread_mutex_lock(&lifecycle);int status=open_location(get_vms,allowed,context);pthread_mutex_unlock(&lifecycle);return status;}
int silph_location_close(void){pthread_mutex_lock(&lifecycle);int status=close_location();pthread_mutex_unlock(&lifecycle);return status;}
