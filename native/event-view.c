#define _GNU_SOURCE
#include "event-actions.h"
#include "visual-tools.h"
#include <shadowhook.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <android/log.h>
typedef struct {
    SilphManagedApi base;const void *(*method)(void *,const char *,int);const void *(*param)(const void *,uint32_t);
    void *(*class_from_type)(const void *);const char *(*name)(void *),*(*space)(void *);
    const void *(*image)(void *),*(*field)(void *,const char *);void *(*klass)(const void *,const char *,const char *);void (*get)(void *,const void *,void *);
    SilphVisualAllowed allowed;void *context,*stub,*selected_stub,*reward_stub,*out_class;uintptr_t address,selected_address,reward_address;unsigned char bytes[16],selected_bytes[16],reward_bytes[16];uint32_t owner;
    const void *reward_status,*reward_loot,*reward_encounter;
    struct {uint64_t id,until;bool captured;} expected[128];
} EventView;
static EventView view;
static void (*enter_original)(void *,void *,const void *);
static void (*selected_original)(void *,void *,bool,const void *);
static void (*reward_original)(void *,void *,void *,const void *);
static uint64_t now_ns(void){struct timespec t={0};return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static bool camera_owned(uint64_t id){if(!id||!view.stub)return false;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&now<view.expected[i].until)return true;return false;}
bool silph_event_owned(uint64_t id){if(!id||!view.stub)return false;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&!view.expected[i].captured&&now<view.expected[i].until)return true;return false;}
int silph_event_expect(uint64_t id){uint64_t now=now_ns();if(!id||!view.stub)return SILPH_MANAGED_ARGUMENT;unsigned slot=128;for(unsigned i=0;i<128;i++){if(view.expected[i].id==id){slot=i;break;}if(slot==128&&view.expected[i].until<=now)slot=i;}if(slot==128)return SILPH_MANAGED_CAPACITY;view.expected[slot].id=id;view.expected[slot].until=now+UINT64_C(120000000000);view.expected[slot].captured=false;return 0;}
void silph_event_complete(uint64_t id){for(unsigned i=0;i<128;i++)if(view.expected[i].id==id){view.expected[i].id=view.expected[i].until=0;view.expected[i].captured=false;}}
void silph_event_captured(uint64_t id){if(!id)return;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&now<view.expected[i].until){view.expected[i].captured=true;view.expected[i].until=now+UINT64_C(120000000000);}}
static bool intercept(void *pokemon){
    if(view.stub&&pokemon&&view.owner==(unsigned)syscall(__NR_gettid)&&view.allowed(view.context)){
        void *klass=view.base.object_get_class(pokemon);const char *name=klass?view.name(klass):NULL,*space=klass?view.space(klass):NULL;
        const void *getter=name&&space&&!strcmp(name,"WildMapPokemon")&&!strcmp(space,"Niantic.Holoholo.Map")?view.method(klass,"get_EncounterId",0):NULL;
        if(getter&&view.base.method_get_param_count(getter)==0&&view.base.type_get_type(view.base.method_get_return_type(getter))==SILPH_READ_U64){
            void *exception=NULL,*boxed=view.base.runtime_invoke(getter,pokemon,NULL,&exception),*value=!exception&&boxed?view.base.object_unbox(boxed):NULL;uint64_t id=0;
            if(value)memcpy(&id,value,8);if(id&&camera_owned(id)){__android_log_print(ANDROID_LOG_INFO,"SilphEvent","event encounter handed to Farmer encounter=%llu",(unsigned long long)id);return true;}
        }
    }
    return false;
}
static void enter(void *map,void *pokemon,const void *method){if(!intercept(pokemon))enter_original(map,pokemon,method);}
static void selected(void *map,void *pokemon,bool animate,const void *method){if(!intercept(pokemon))selected_original(map,pokemon,animate,method);}
static void reward(void *service,void *tappable,void *reply,const void *method){
    if(view.reward_stub&&tappable&&reply&&view.owner==(unsigned)syscall(__NR_gettid)&&view.allowed(view.context)&&view.base.object_get_class(reply)==view.out_class){
        const void *getter=view.method(view.base.object_get_class(tappable),"get_EncounterId",0);void *exception=NULL,*boxed=getter?view.base.runtime_invoke(getter,tappable,NULL,&exception):NULL,*value=!exception&&boxed?view.base.object_unbox(boxed):NULL;uint64_t id=0;if(value)memcpy(&id,value,8);
        if(silph_event_owned(id)){int32_t status=0,count=0;void *loot=NULL,*encounter=NULL;view.get(reply,view.reward_status,&status);view.get(reply,view.reward_loot,&loot);view.get(reply,view.reward_encounter,&encounter);
            const void *size=loot?view.method(view.base.object_get_class(loot),"get_Count",0):NULL;exception=NULL;boxed=size?view.base.runtime_invoke(size,loot,NULL,&exception):NULL;value=!exception&&boxed?view.base.object_unbox(boxed):NULL;if(value)memcpy(&count,value,4);
            __android_log_print(ANDROID_LOG_INFO,"SilphEvent","reward received encounter=%llu status=%d bundles=%d pokemon=%u",(unsigned long long)id,status,count,!!encounter);
        }
    }
    reward_original(service,tappable,reply,method);
}
int silph_event_view_open(const SilphManagedApi *base,SilphManagedResolver resolve,void *context,uint32_t offset,void *map,SilphVisualAllowed allowed,SilphVisualAddress address){
    if(view.stub)return 0;if(!base||!resolve||!map||!allowed||!address||offset>120||offset%sizeof(void *))return SILPH_MANAGED_ARGUMENT;
    memset(&view,0,sizeof(view));view.base=*base;view.allowed=allowed;view.context=context;view.owner=(unsigned)syscall(__NR_gettid);
#define LOAD(member,name) do{void *p=resolve("il2cpp_" name,context);if(!p)return SILPH_MANAGED_ABI_MISMATCH;memcpy(&view.member,&p,sizeof(p));}while(0)
    LOAD(method,"class_get_method_from_name");LOAD(param,"method_get_param");LOAD(class_from_type,"class_from_type");LOAD(name,"class_get_name");LOAD(space,"class_get_namespace");
    LOAD(image,"class_get_image");LOAD(klass,"class_from_name");LOAD(field,"class_get_field_from_name");LOAD(get,"field_get_value");
#undef LOAD
    void *klass=base->object_get_class(map);const void *method=view.method(klass,"EnterPokemonEncounter",1);uint32_t flags=0;
    if(!method||base->method_get_param_count(method)!=1||base->type_get_type(base->method_get_return_type(method))!=SILPH_READ_VOID||(base->method_get_flags(method,&flags)&0x10)||base->method_is_generic(method))return SILPH_MANAGED_CONTRACT;
    void *type=view.class_from_type(view.param(method,0));const char *name=type?view.name(type):NULL,*space=type?view.space(type):NULL;
    if(!name||!space||strcmp(name,"MapPokemon")||strcmp(space,"Niantic.Holoholo.Map"))return SILPH_MANAGED_CONTRACT;
    memcpy(&view.address,(const char *)method+offset,sizeof(view.address));if((view.address&3)||!address(view.address,context))return SILPH_MANAGED_CONTRACT;
    const void *selection=view.method(klass,"PokemonSelected",2);if(!selection||base->method_get_param_count(selection)!=2||base->type_get_type(base->method_get_return_type(selection))!=SILPH_READ_VOID||base->type_get_type(view.param(selection,1))!=SILPH_READ_BOOL||view.class_from_type(view.param(selection,0))!=type)return SILPH_MANAGED_CONTRACT;
    memcpy(&view.selected_address,(const char *)selection+offset,sizeof(view.selected_address));if((view.selected_address&3)||!address(view.selected_address,context))return SILPH_MANAGED_CONTRACT;
    memcpy(view.bytes,(void *)view.address,16);memcpy(view.selected_bytes,(void *)view.selected_address,16);view.stub=shadowhook_hook_func_addr((void *)view.address,(void *)enter,(void **)&enter_original);if(!view.stub)return SILPH_MANAGED_STATE;
    view.selected_stub=shadowhook_hook_func_addr((void *)view.selected_address,(void *)selected,(void **)&selected_original);if(!view.selected_stub){silph_event_view_close();return SILPH_MANAGED_STATE;}
    void *behavior=view.klass(view.image(klass),"Niantic.Holoholo.Services.DynamicTappables","DynamicTappableBehaviorService");const void *receive=behavior?view.method(behavior,"bfvy",2):NULL;
    if(receive&&base->method_get_param_count(receive)==2&&base->type_get_type(base->method_get_return_type(receive))==SILPH_READ_VOID){
        view.out_class=view.class_from_type(view.param(receive,1));name=view.out_class?view.name(view.out_class):NULL;space=view.out_class?view.space(view.out_class):NULL;
        if(name&&space&&!strcmp(name,"ProcessTappableOutProto")&&!strcmp(space,"Holoholo.Rpc")){
            view.reward_status=view.field(view.out_class,"Status");view.reward_loot=view.field(view.out_class,"Reward");view.reward_encounter=view.field(view.out_class,"Encounter");memcpy(&view.reward_address,(const char *)receive+offset,sizeof(view.reward_address));
            if(view.reward_status&&view.reward_loot&&view.reward_encounter&&!(view.reward_address&3)&&address(view.reward_address,context)){memcpy(view.reward_bytes,(void *)view.reward_address,16);view.reward_stub=shadowhook_hook_func_addr((void *)view.reward_address,(void *)reward,(void **)&reward_original);}
        }
    }
    return 0;
}
int silph_event_view_close(void){if(!view.stub)return 0;if(view.owner!=(unsigned)syscall(__NR_gettid))return SILPH_MANAGED_THREAD;
    memset(view.expected,0,sizeof(view.expected));if(view.selected_stub){if(shadowhook_unhook(view.selected_stub)||memcmp((void *)view.selected_address,view.selected_bytes,16))return SILPH_MANAGED_STATE;view.selected_stub=NULL;}
    if(view.reward_stub){if(shadowhook_unhook(view.reward_stub)||memcmp((void *)view.reward_address,view.reward_bytes,16))return SILPH_MANAGED_STATE;view.reward_stub=NULL;}
    if(shadowhook_unhook(view.stub)||memcmp((void *)view.address,view.bytes,16))return SILPH_MANAGED_STATE;memset(&view,0,sizeof(view));return 0;}
