#define _GNU_SOURCE
#include "event-actions.h"
#include "visual-tools.h"
#include "callback-memory.h"
#include <shadowhook.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <android/log.h>
typedef struct {
    SilphManagedApi base;const void *(*method)(void *,const char *,int);const void *(*param)(const void *,uint32_t);
    void *(*class_from_type)(const void *);const char *(*name)(void *),*(*space)(void *);
    const void *(*class_image)(void *);void *(*klass)(const void *,const char *,const char *);
    SilphVisualAllowed allowed;void *context,*stub,*selected_stub;uintptr_t address,selected_address;unsigned char bytes[16],selected_bytes[16];uint32_t owner;
    struct {uint64_t id,until;bool captured;} expected[128];void *map_class,*wild;const void *encounter_getter;
} EventView;
static EventView view;
void *silph_event_enter_original,*silph_event_selected_original;
extern void silph_event_enter_gate(void),silph_event_selected_gate(void);
static uint64_t now_ns(void){struct timespec t={0};return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static bool camera_owned(uint64_t id){if(!id||!view.stub)return false;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&now<view.expected[i].until)return true;return false;}
bool silph_event_owned(uint64_t id){if(!id||!view.stub)return false;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&!view.expected[i].captured&&now<view.expected[i].until)return true;return false;}
int silph_event_expect(uint64_t id){uint64_t now=now_ns();if(!id||!view.stub)return SILPH_MANAGED_ARGUMENT;unsigned slot=128;for(unsigned i=0;i<128;i++){if(view.expected[i].id==id){slot=i;break;}if(slot==128&&view.expected[i].until<=now)slot=i;}if(slot==128)return SILPH_MANAGED_CAPACITY;view.expected[slot].id=id;view.expected[slot].until=now+UINT64_C(120000000000);view.expected[slot].captured=false;return 0;}
void silph_event_complete(uint64_t id){for(unsigned i=0;i<128;i++)if(view.expected[i].id==id){view.expected[i].id=view.expected[i].until=0;view.expected[i].captured=false;}}
void silph_event_captured(uint64_t id){if(!id)return;uint64_t now=now_ns();for(unsigned i=0;i<128;i++)if(view.expected[i].id==id&&now<view.expected[i].until){view.expected[i].captured=true;view.expected[i].until=now+UINT64_C(120000000000);}}
static bool intercept(void *pokemon){
    if(view.stub&&pokemon&&view.owner==(unsigned)syscall(__NR_gettid)&&view.allowed(view.context)){
        const void *getter=silph_callback_object(pokemon,view.wild)?view.encounter_getter:NULL;
        if(getter){
            void *exception=NULL,*boxed=view.base.runtime_invoke(getter,pokemon,NULL,&exception),*value=!exception&&boxed?view.base.object_unbox(boxed):NULL;uint64_t id=0;
            if(value)memcpy(&id,value,8);if(id&&camera_owned(id)){__android_log_print(ANDROID_LOG_INFO,"SilphEvent","event encounter handed to Farmer encounter=%llu",(unsigned long long)id);return true;}
        }
    }
    return false;
}
int silph_event_enter_decide(SilphCallbackFrame *frame){return frame&&view.stub&&view.owner==(unsigned)syscall(__NR_gettid)&&silph_callback_object((void *)(uintptr_t)frame->regs[0],view.map_class)&&intercept((void *)(uintptr_t)frame->regs[1]);}
int silph_event_view_open(const SilphManagedApi *base,SilphManagedResolver resolve,void *context,uint32_t offset,void *map,SilphVisualAllowed allowed,SilphVisualAddress address){
    if(view.stub)return 0;if(!base||!resolve||!map||!allowed||!address||offset>120||offset%sizeof(void *))return SILPH_MANAGED_ARGUMENT;
    memset(&view,0,sizeof(view));view.base=*base;view.allowed=allowed;view.context=context;view.owner=(unsigned)syscall(__NR_gettid);
#define LOAD(member,name) do{void *p=resolve("il2cpp_" name,context);if(!p)return SILPH_MANAGED_ABI_MISMATCH;memcpy(&view.member,&p,sizeof(p));}while(0)
    LOAD(method,"class_get_method_from_name");LOAD(param,"method_get_param");LOAD(class_from_type,"class_from_type");LOAD(name,"class_get_name");LOAD(space,"class_get_namespace");LOAD(class_image,"class_get_image");LOAD(klass,"class_from_name");
#undef LOAD
    void *klass=base->object_get_class(map);view.map_class=klass;view.wild=view.klass(view.class_image(klass),"Niantic.Holoholo.Map","WildMapPokemon");
    view.encounter_getter=view.wild?view.method(view.wild,"get_EncounterId",0):NULL;
    if(!view.encounter_getter||base->method_get_param_count(view.encounter_getter)||base->type_get_type(base->method_get_return_type(view.encounter_getter))!=SILPH_READ_U64)return SILPH_MANAGED_CONTRACT;
    const void *method=view.method(klass,"EnterPokemonEncounter",1);uint32_t flags=0;
    if(!method||base->method_get_param_count(method)!=1||base->type_get_type(base->method_get_return_type(method))!=SILPH_READ_VOID||(base->method_get_flags(method,&flags)&0x10)||base->method_is_generic(method))return SILPH_MANAGED_CONTRACT;
    void *type=view.class_from_type(view.param(method,0));const char *name=type?view.name(type):NULL,*space=type?view.space(type):NULL;
    if(!name||!space||strcmp(name,"MapPokemon")||strcmp(space,"Niantic.Holoholo.Map"))return SILPH_MANAGED_CONTRACT;
    memcpy(&view.address,(const char *)method+offset,sizeof(view.address));if((view.address&3)||!address(view.address,context))return SILPH_MANAGED_CONTRACT;
    const void *selection=view.method(klass,"PokemonSelected",2);if(!selection||base->method_get_param_count(selection)!=2||base->type_get_type(base->method_get_return_type(selection))!=SILPH_READ_VOID||base->type_get_type(view.param(selection,1))!=SILPH_READ_BOOL||view.class_from_type(view.param(selection,0))!=type)return SILPH_MANAGED_CONTRACT;
    memcpy(&view.selected_address,(const char *)selection+offset,sizeof(view.selected_address));if((view.selected_address&3)||!address(view.selected_address,context))return SILPH_MANAGED_CONTRACT;
    memcpy(view.bytes,(void *)view.address,16);memcpy(view.selected_bytes,(void *)view.selected_address,16);view.stub=shadowhook_hook_func_addr((void *)view.address,(void *)silph_event_enter_gate,&silph_event_enter_original);if(!view.stub)return SILPH_MANAGED_STATE;
    view.selected_stub=shadowhook_hook_func_addr((void *)view.selected_address,(void *)silph_event_selected_gate,&silph_event_selected_original);if(!view.selected_stub){silph_event_view_close();return SILPH_MANAGED_STATE;}
    return 0;
}
int silph_event_view_close(void){if(!view.stub)return 0;if(view.owner!=(unsigned)syscall(__NR_gettid))return SILPH_MANAGED_THREAD;
    memset(view.expected,0,sizeof(view.expected));if(view.selected_stub){if(shadowhook_unhook(view.selected_stub)||memcmp((void *)view.selected_address,view.selected_bytes,16))return SILPH_MANAGED_STATE;view.selected_stub=NULL;}
    if(shadowhook_unhook(view.stub)||memcmp((void *)view.address,view.bytes,16))return SILPH_MANAGED_STATE;memset(&view,0,sizeof(view));return 0;}
