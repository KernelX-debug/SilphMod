#define _GNU_SOURCE
#include "rocket-flow.h"
#include <shadowhook.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <android/log.h>

typedef struct {
    const void **(*assemblies)(void *,size_t *);const void *(*image)(const void *);const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *),*(*nested)(void *,void **),*(*class_from_type)(const void *);
    const char *(*class_name)(void *);bool (*is_enum)(void *),(*assignable)(void *,void *);const void *(*enum_base)(void *);
    const void *(*method)(void *,const char *,int),*(*parameter)(const void *,uint32_t),*(*field)(void *,const char *),*(*field_type)(const void *),*(*fields)(void *,void **);
    uint32_t (*field_flags)(const void *);const char *(*type_name)(const void *);void (*free)(void *);
    void (*get)(void *,const void *,void *),(*set)(void *,const void *,void *),(*set_object)(void *,const void *,void *);const uint16_t *(*chars)(void *);
} FlowApi;
typedef struct {void *stub;uintptr_t address;unsigned char before[16];} FlowHook;
typedef struct {SilphManagedRef object;float before[3];} FlowTiming;
typedef struct {
    SilphManagedApi base;FlowApi api;void *context,*managed,*allowed_context;SilphVisualAllowed allowed;SilphVisualAddress address;
    unsigned owner,closed,depth,features,guards,grunt_character;uint32_t offset;FlowHook hooks[5];unsigned used;uint64_t callbacks[5];SilphSchedulePermit permit;
    void *service_class,*state_class,*dialog_class,*action_class,*prep_class,*prep_data_class,*data_class,*summary_class,*iterator_class,*wait_class;
    const void *incident_id,*incident_state,*invoke_action,*challenge,*gui_alive,*gui_state,*prep_fields[3],*iterator_owner,*iterator_current,*wait_duration;
    void *conversation_class,*config_class,*state_api_class;const void *state_config,*state_api,*config_character,*config_mode,*complete_conversation;
    SilphManagedRef service;char incident[161];uint64_t request,dialogues,challenges,waits;uintptr_t challenged;
    FlowTiming timings[12];unsigned timing_count;
} Flow;
static _Atomic(Flow *) current;
static void (*conversation_original)(void *,void *,void *,const void *),(*challenge_original)(void *,const void *),(*prepare_original)(void *,void *,const void *);
static bool (*summary_original)(void *,const void *);
static void (*dialog_state_original)(void *,void *,void *,const void *);
static Flow *owned(void){Flow *s=atomic_load_explicit(&current,memory_order_acquire);return s&&!s->closed&&s->owner==(unsigned)syscall(__NR_gettid)?s:NULL;}
typedef struct {Flow *state;unsigned depth;} FlowScope;
static FlowScope scope(Flow *s){FlowScope out={s,s?s->depth:0};if(s)s->depth++;return out;}
static void leave(FlowScope *s){if(s->state)s->state->depth=s->depth;}
#define FLOW_SCOPE(s) FlowScope flow_scope __attribute__((unused,cleanup(leave)))=scope(s)
bool silph_rocket_flow_busy(void){Flow *s=owned();return s&&s->depth;}
size_t silph_rocket_flow_size(void){return sizeof(Flow);}
static void release(Flow *s,SilphManagedRef *ref){if(ref->serial){silph_managed_release(s->managed,*ref);*ref=(SilphManagedRef){0};}}
static void *klass(Flow *s,const char *assembly,const char *space,const char *name){size_t n=0;const void **a=s->api.assemblies(s->base.domain_get(),&n);if(!a||!n||n>2048)return NULL;for(size_t i=0;i<n;i++){const void *image=s->api.image(a[i]);const char *actual=image?s->api.image_name(image):NULL;if(actual&&!strcmp(actual,assembly))return s->api.klass(image,space,name);}return NULL;}
static bool type(Flow *s,const void *t,const char *expected){const char *name=t?s->api.type_name(t):NULL;bool equal=name&&!strcmp(name,expected);if(name)s->api.free((void *)name);return equal;}
static void *nested_class(Flow *s,void *parent,const char *wanted){void *iterator=NULL;for(unsigned i=0;parent&&i<128;i++){void *k=s->api.nested(parent,&iterator);if(!k)break;const char *name=s->api.class_name(k);if(name&&!strcmp(name,wanted))return k;}return NULL;}
static bool exact(Flow *s,void *o,void *k){return o&&k&&s->base.object_get_class(o)==k;}
static const void *field(Flow *s,void *k,const char *name,const char *expected){const void *f=k?s->api.field(k,name):NULL;return f&&!(s->api.field_flags(f)&0x50u)&&type(s,s->api.field_type(f),expected)?f:NULL;}
static const void *method(Flow *s,void *k,const char *name,const char *result,unsigned count,const char **args){const void *m=k?s->api.method(k,name,(int)count):NULL;uint32_t impl=0;if(!m||s->base.method_get_param_count(m)!=count||(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||s->base.method_is_inflated(m)||(result&&!type(s,s->base.method_get_return_type(m),result))){__android_log_print(ANDROID_LOG_WARN,"SilphRocket","presentation method unavailable name=%s found=%u",name,m!=NULL);return NULL;}for(unsigned i=0;i<count;i++)if(!type(s,s->api.parameter(m,i),args[i])){__android_log_print(ANDROID_LOG_WARN,"SilphRocket","presentation parameter unavailable name=%s index=%u",name,i);return NULL;}return m;}
static bool invoke(Flow *s,const void *m,void *o,void **args,void **out){if(!m)return false;void *exception=NULL,*value=s->base.runtime_invoke(m,o,args,&exception);if(out)*out=value;return !exception;}
static bool guard(Flow *s,unsigned code,int detail){if(s&&s->request&&code<16&&!(s->guards&(1u<<code))){s->guards|=1u<<code;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","presentation waiting request=%llu guard=%u detail=%d",(unsigned long long)s->request,code,detail);}return false;}
static bool bound(Flow *s,void **service){
    if(!s||!s->request)return false;if(s->depth>3)return guard(s,1,(int)s->depth);if(!silph_schedule_permitted(&s->permit,true)&&!s->allowed(s->allowed_context))return guard(s,2,0);if(silph_managed_target(s->managed,s->service,service)||!exact(s,*service,s->service_class))return guard(s,3,0);
    void *name=NULL;if(!invoke(s,s->incident_id,*service,NULL,&name)||!name)return guard(s,4,0);int32_t n=s->base.string_length(name);size_t bytes=strlen(s->incident);if(n<1||(size_t)n!=bytes)return guard(s,5,n);
    const uint16_t *chars=s->api.chars(name);if(!chars)return guard(s,6,0);for(size_t i=0;i<bytes;i++)if(chars[i]!=(unsigned char)s->incident[i])return guard(s,7,0);
    return (s->grunt_character>=4&&s->grunt_character<=39)||(s->grunt_character>=47&&s->grunt_character<=50);
}
static bool gui_open(Flow *s,void *o){uintptr_t native=0;int32_t state=0;s->api.get(o,s->gui_alive,&native);s->api.get(o,s->gui_state,&state);return native&&state==2;}
static void dialog_state(void *o,void *api,void *config,const void *m){
    Flow *s=owned();FLOW_SCOPE(s);if(s)s->callbacks[4]++;void *service=NULL;
    if(bound(s,&service)&&exact(s,o,s->conversation_class)&&exact(s,config,s->config_class)&&api&&s->api.assignable(s->state_api_class,s->base.object_get_class(api))){
        int32_t mode=-1,character=0;s->api.get(config,s->config_mode,&mode);s->api.get(config,s->config_character,&character);
        if(mode>=0&&mode<=4&&(character==0||(character>=4&&character<=39)||(character>=47&&character<=50))){
            s->api.set_object(o,s->state_config,config);s->api.set_object(o,s->state_api,api);void *stored_config=NULL,*stored_api=NULL;s->api.get(o,s->state_config,&stored_config);s->api.get(o,s->state_api,&stored_api);
            if(stored_config==config&&stored_api==api){bool completed=invoke(s,s->complete_conversation,o,NULL,NULL);
                __android_log_print(completed?ANDROID_LOG_INFO:ANDROID_LOG_WARN,"SilphRocket","dialogue state completed request=%llu mode=%d completed=%u",(unsigned long long)s->request,mode,completed);s->dialogues++;return;}
        }
    }
    dialog_state_original(o,api,config,m);
}
static void conversation(void *o,void *proto,void *complete,const void *m){
    Flow *s=owned();FLOW_SCOPE(s);if(s)s->callbacks[0]++;void *service=NULL,*state=NULL;
    if(bound(s,&service)&&exact(s,o,s->state_class)&&exact(s,complete,s->action_class)&&exact(s,proto,s->data_class)
        &&invoke(s,s->incident_state,service,NULL,&state)&&state==o){
        bool completed=invoke(s,s->invoke_action,complete,NULL,NULL);
        __android_log_print(completed?ANDROID_LOG_INFO:ANDROID_LOG_WARN,"SilphRocket","dialogue transition request=%llu count=%llu completed=%u",(unsigned long long)s->request,(unsigned long long)++s->dialogues,completed);return;
    }
    conversation_original(o,proto,complete,m);
}
static void challenge_complete(void *o,const void *m){
    Flow *s=owned();FLOW_SCOPE(s);if(s)s->callbacks[1]++;challenge_original(o,m);void *service=NULL;
    if(bound(s,&service)&&exact(s,o,s->dialog_class)&&s->challenged!=(uintptr_t)o&&gui_open(s,o)&&invoke(s,s->challenge,o,NULL,NULL)){
        s->challenged=(uintptr_t)o;s->challenges++;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","challenge advanced when ready request=%llu",(unsigned long long)s->request);
    }
}
static void prepare(void *o,void *data,const void *m){
    Flow *s=owned();FLOW_SCOPE(s);if(s)s->callbacks[2]++;void *service=NULL;
    if(bound(s,&service)&&exact(s,o,s->prep_class)&&exact(s,data,s->prep_data_class)){
        FlowTiming *record=NULL;for(unsigned i=0;i<s->timing_count;i++){void *old=NULL;if(!silph_managed_target(s->managed,s->timings[i].object,&old)&&old==o){record=&s->timings[i];break;}if(!old&&!record){release(s,&s->timings[i].object);record=&s->timings[i];}}
        if(!record&&s->timing_count<12)record=&s->timings[s->timing_count++];
        if(record&&!record->object.serial){float values[3]={0};bool valid=true;for(unsigned i=0;i<3;i++){s->api.get(o,s->prep_fields[i],&values[i]);valid=valid&&isfinite(values[i])&&values[i]>=0&&values[i]<=120;}
            if(valid&&!silph_managed_retain(s->managed,o,true,&record->object)){memcpy(record->before,values,sizeof(values));float zero=0;for(unsigned i=0;i<3;i++)s->api.set(o,s->prep_fields[i],&zero);}}
    }
    prepare_original(o,data,m);
}
static bool summary_next(void *o,const void *m){
    Flow *s=owned();FLOW_SCOPE(s);if(s)s->callbacks[3]++;bool result=summary_original(o,m);void *service=NULL;
    if(result&&bound(s,&service)&&exact(s,o,s->iterator_class)){void *owner=NULL,*wait=NULL;s->api.get(o,s->iterator_owner,&owner);s->api.get(o,s->iterator_current,&wait);
        if(exact(s,owner,s->summary_class)&&exact(s,wait,s->wait_class)){float duration=0;s->api.get(wait,s->wait_duration,&duration);if(isfinite(duration)&&duration>.02f&&duration<=10){float short_wait=.01f;s->api.set(wait,s->wait_duration,&short_wait);s->waits++;}}}
    return result;
}
static bool hook(Flow *s,const void *m,void *proxy,void **original){if(!m||s->used>=5)return false;uintptr_t at=0;memcpy(&at,(const char *)m+s->offset,sizeof(at));if((at&3)||!s->address(at,s->context))return false;for(unsigned i=0;i<s->used;i++)if(s->hooks[i].address==at)return false;FlowHook *h=&s->hooks[s->used];h->address=at;memcpy(h->before,(void *)at,16);h->stub=shadowhook_hook_func_addr((void *)at,proxy,original);if(!h->stub)return false;s->used++;return true;}
int silph_rocket_flow_open(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolve,void *context,uint32_t offset,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address){
    if(!memory||bytes!=sizeof(Flow)||!base||!resolve||!allowed||!address||offset>120||offset%sizeof(void *)||atomic_load(&current))return 1;Flow *s=memory;memset(s,0,bytes);s->base=*base;s->owner=(unsigned)syscall(__NR_gettid);s->context=context;s->allowed=allowed;s->allowed_context=allowed_context;s->address=address;s->offset=offset;
#define LOAD(member,name) do{void *p=resolve("il2cpp_" name,context);if(!p){__android_log_print(ANDROID_LOG_WARN,"SilphRocket","presentation API unavailable name=%s",name);return 1;}memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(nested,"class_get_nested_types");LOAD(class_from_type,"class_from_type");LOAD(class_name,"class_get_name");LOAD(is_enum,"class_is_enum");LOAD(assignable,"class_is_assignable_from");LOAD(enum_base,"class_enum_basetype");LOAD(method,"class_get_method_from_name");LOAD(parameter,"method_get_param");LOAD(field,"class_get_field_from_name");LOAD(field_type,"field_get_type");LOAD(fields,"class_get_fields");LOAD(field_flags,"field_get_flags");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(get,"field_get_value");LOAD(set,"field_set_value");LOAD(set_object,"field_set_value_object");LOAD(chars,"string_chars");
#undef LOAD
    s->service_class=klass(s,"holo-game.dll","Niantic.Holoholo.Incident","IncidentService");s->state_class=klass(s,"holo-game.dll","Niantic.Holoholo.Incident","IncidentInvasionState");s->dialog_class=klass(s,"holo-game.dll","Niantic.Holoholo.Combat.NpcTrainers","NpcTrainerInterstitialGui");s->prep_class=klass(s,"holo-game.dll","Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController");s->summary_class=klass(s,"holo-game.dll","Niantic.Holoholo.Incident","IncidentCombatResolveGuiController");
    s->action_class=klass(s,"mscorlib.dll","System","Action");s->data_class=klass(s,"holo-protos.dll","Holoholo.Rpc","ClientPokestopNpcDialogueStepProto");s->wait_class=klass(s,"Zeppelin.dll","Niantic.Zeppelin.Scheduler","Wait");
    s->incident_id=method(s,s->service_class,"get_CurrentIncidentId","System.String",0,NULL);s->incident_state=method(s,s->service_class,"get_IncidentState","Niantic.Holoholo.Incident.IIncidentActionState",0,NULL);s->invoke_action=method(s,s->action_class,"Invoke","System.Void",0,NULL);s->challenge=method(s,s->dialog_class,"PressChallenge","System.Void",0,NULL);
    void *gui=klass(s,"holo-game.dll","Niantic.Holoholo.NUI","GuiController");s->gui_alive=field(s,gui,"m_CachedPtr","System.IntPtr");s->gui_state=gui?s->api.field(gui,"currentState"):NULL;
    void *state_class=s->gui_state?s->api.class_from_type(s->api.field_type(s->gui_state)):NULL;
    if(!s->gui_state||(s->api.field_flags(s->gui_state)&0x50u)||!state_class||!s->api.is_enum(state_class)||!type(s,s->api.enum_base(state_class),"System.Int32"))s->gui_state=NULL;
    if(!s->incident_id||!s->incident_state||!s->invoke_action||!s->gui_alive||!s->gui_state||!s->state_class||!s->action_class||!s->data_class){unsigned missing=(!s->incident_id)|(!s->incident_state)<<1|(!s->invoke_action)<<3|(!s->gui_alive)<<4|(!s->gui_state)<<5|(!s->state_class)<<6|(!s->action_class)<<7|(!s->data_class)<<8;__android_log_print(ANDROID_LOG_WARN,"SilphRocket","presentation contract unavailable missing=%u",missing);return 1;}
    s->managed=calloc(1,silph_managed_size(32));if(!s->managed)return 1;int context_status=silph_managed_open(s->managed,silph_managed_size(32),base,32,64);if(context_status){free(s->managed);s->managed=NULL;__android_log_print(ANDROID_LOG_WARN,"SilphRocket","presentation context unavailable error=%d",context_status);return 1;}atomic_store_explicit(&current,s,memory_order_release);
    const char *conversation_args[]={"Holoholo.Rpc.ClientPokestopNpcDialogueStepProto","System.Action"};const void *converse=method(s,s->state_class,"StartConversation","System.Void",2,conversation_args);
    if(hook(s,converse,(void *)conversation,(void **)&conversation_original))s->features|=1;
    const void *opened=method(s,s->dialog_class,"HandleOpenComplete","System.Void",0,NULL);if(s->challenge&&hook(s,opened,(void *)challenge_complete,(void **)&challenge_original))s->features|=2;
    const char *fields[]={"backgroundTransitionWaitDurationSec","versusScreenDurationSec","combatTransitionDurationSec"};bool valid=true;for(unsigned i=0;i<3;i++){s->prep_fields[i]=field(s,s->prep_class,fields[i],"System.Single");valid=valid&&s->prep_fields[i];}
    s->prep_data_class=nested_class(s,s->prep_class,"Data");
    const void *initialize=s->prep_class?s->api.method(s->prep_class,"Initialize",1):NULL;uint32_t impl=0;
    if(valid&&initialize&&s->prep_data_class&&s->base.method_get_param_count(initialize)==1&&!(s->base.method_get_flags(initialize,&impl)&0x10u)&&!s->base.method_is_generic(initialize)&&!s->base.method_is_inflated(initialize)&&type(s,s->base.method_get_return_type(initialize),"System.Void")&&s->api.class_from_type(s->api.parameter(initialize,0))==s->prep_data_class&&hook(s,initialize,(void *)prepare,(void **)&prepare_original))s->features|=4;
    void *iterator=NULL;for(unsigned i=0;s->summary_class&&i<128;i++){void *k=s->api.nested(s->summary_class,&iterator);if(!k)break;const void *owner=field(s,k,"<>4__this","Niantic.Holoholo.Incident.IncidentCombatResolveGuiController"),*step=method(s,k,"MoveNext","System.Boolean",0,NULL),*current_field=NULL;void *it=NULL;for(unsigned j=0;owner&&step&&j<32;j++){const void *f=s->api.fields(k,&it);if(!f)break;if(!(s->api.field_flags(f)&0x50u)&&type(s,s->api.field_type(f),"Niantic.Zeppelin.Scheduler.ISchedule")){if(current_field){current_field=NULL;break;}current_field=f;}}
        if(owner&&step&&current_field){s->iterator_class=k;s->iterator_owner=owner;s->iterator_current=current_field;break;}}
    s->wait_duration=field(s,s->wait_class,"<SleepFor>k__BackingField","System.Single");const void *step=method(s,s->iterator_class,"MoveNext","System.Boolean",0,NULL);if(s->wait_duration&&hook(s,step,(void *)summary_next,(void **)&summary_original))s->features|=8;
    s->conversation_class=klass(s,"holo-game.dll","Niantic.Holoholo.Incident","IncidentInvasionDialogState");s->config_class=klass(s,"holo-game.dll","Niantic.Holoholo.Incident","IncidentInvasionDialogStateConfig");s->state_api_class=klass(s,"Niantic.Platform.GameStates.dll","Niantic.Platform.GameStates.StateLogic","IStateApi");
    unsigned configs=0,apis=0;void *it=NULL;for(unsigned i=0;s->conversation_class&&i<64;i++){const void *f=s->api.fields(s->conversation_class,&it);if(!f)break;if(s->api.field_flags(f)&0x50u)continue;void *k=s->api.class_from_type(s->api.field_type(f));if(k==s->config_class){s->state_config=f;configs++;}if(k==s->state_api_class){s->state_api=f;apis++;}}
    if(configs!=1)s->state_config=NULL;if(apis!=1)s->state_api=NULL;
    s->config_character=s->config_class?s->api.field(s->config_class,"Character"):NULL;s->config_mode=s->config_class?s->api.field(s->config_class,"DialogMode"):NULL;
    const void *enums[]={s->config_character,s->config_mode};for(unsigned i=0;i<2;i++){const void *f=enums[i];void *k=f?s->api.class_from_type(s->api.field_type(f)):NULL;if(!f||(s->api.field_flags(f)&0x50u)||!k||!s->api.is_enum(k)||!type(s,s->api.enum_base(k),"System.Int32")){s->config_character=NULL;s->config_mode=NULL;}}
    s->complete_conversation=method(s,s->conversation_class,"ConvoCompleted","System.Void",0,NULL);const char *state_args[]={"Niantic.Platform.GameStates.StateLogic.IStateApi","Niantic.Holoholo.Incident.IncidentInvasionDialogStateConfig"};const void *enter=method(s,s->conversation_class,"EnterState","System.Void",2,state_args);
    if(s->state_api_class&&s->state_api&&s->state_config&&s->config_character&&s->config_mode&&s->complete_conversation&&hook(s,enter,(void *)dialog_state,(void **)&dialog_state_original))s->features|=16;
    __android_log_print(ANDROID_LOG_INFO,"SilphRocket","scoped presentation ready features=%u hooks=%u",s->features,s->used);return 0;
}
int silph_rocket_flow_bind(void *managed,SilphManagedRef service,const char *incident,uint64_t request,uint32_t character){Flow *s=owned();if(!s)return 0;if(!managed||!incident||!request||!((character>=4&&character<=39)||(character>=47&&character<=50))||!incident[0]||strnlen(incident,161)>160)return 1;void *object=NULL;if(silph_managed_target(managed,service,&object)||!exact(s,object,s->service_class))return 1;release(s,&s->service);if(silph_managed_retain(s->managed,object,false,&s->service))return 1;memcpy(s->incident,incident,strlen(incident)+1);s->request=request;s->challenged=0;s->guards=0;s->grunt_character=character;return 0;}
void silph_rocket_flow_permit(const SilphSchedulePermit *permit){Flow *s=owned();if(s)s->permit=permit?*permit:(SilphSchedulePermit){0};}
void silph_rocket_flow_unbind(void){Flow *s=owned();if(s){s->request=0;s->grunt_character=0;s->incident[0]=0;s->challenged=0;release(s,&s->service);}}
bool silph_rocket_flow_challenged(void *managed,SilphManagedRef gui){Flow *s=owned();void *object=NULL;return s&&s->request&&s->challenged&&!silph_managed_target(managed,gui,&object)&&s->challenged==(uintptr_t)object;}
int silph_rocket_flow_close(void *memory){Flow *s=memory;if(!s||s!=owned()||s->depth)return 1;silph_rocket_flow_unbind();s->closed=1;int result=0;for(unsigned i=0;i<s->used;i++){FlowHook *h=&s->hooks[i];if(h->stub){if(shadowhook_unhook(h->stub)||memcmp((void *)h->address,h->before,16))result=1;else h->stub=NULL;}}if(result)return result;
    for(unsigned i=0;i<s->timing_count;i++){void *object=NULL;FlowTiming *r=&s->timings[i];if(r->object.serial&&!silph_managed_target(s->managed,r->object,&object)&&exact(s,object,s->prep_class)){uintptr_t native=0;s->api.get(object,s->gui_alive,&native);if(native)for(unsigned j=0;j<3;j++){float value=0;s->api.get(object,s->prep_fields[j],&value);if(value==0)s->api.set(object,s->prep_fields[j],&r->before[j]);}}release(s,&r->object);}
    SilphManagedMetrics metrics={0};result=silph_managed_close(s->managed,&metrics);if(!result){free(s->managed);s->managed=NULL;atomic_store(&current,NULL);}__android_log_print(ANDROID_LOG_INFO,"SilphRocket","presentation cleanup dialogues=%llu challenges=%llu waits=%llu remaining=%u error=%d callbacks=%llu/%llu/%llu/%llu/%llu",(unsigned long long)s->dialogues,(unsigned long long)s->challenges,(unsigned long long)s->waits,metrics.live_refs,result,(unsigned long long)s->callbacks[0],(unsigned long long)s->callbacks[1],(unsigned long long)s->callbacks[2],(unsigned long long)s->callbacks[3],(unsigned long long)s->callbacks[4]);return result;
}
