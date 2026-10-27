#define _GNU_SOURCE
#include "managed-read.h"
#include <stdatomic.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifdef SILPH_ENABLE_ACTIONS
#include "managed-action.h"
#include "core.h"
#include <math.h>
#ifdef SILPH_PRESENT_SCOPED
#include <android/log.h>
#endif
#include "managed-proto.h"
#include "research-bridge.h"
#include "gift-actions.h"
#include "pass-actions.h"
#endif

#define READ_MAGIC UINT32_C(0x53524431)
typedef struct {
    const void **(*domain_get_assemblies)(void *,size_t *);
    const void *(*assembly_get_image)(const void *);
    const char *(*image_get_name)(const void *);
    void *(*class_from_name)(const void *,const char *,const char *);
    void *(*class_from_type)(const void *);
    const char *(*class_get_name)(void *);
    const char *(*class_get_namespace)(void *);
    void *(*class_get_parent)(void *);
    bool (*class_is_valuetype)(void *);
    bool (*class_is_enum)(void *);
    bool (*class_is_assignable_from)(void *,void *);
    const void *(*class_enum_basetype)(void *);
    const void *(*class_get_fields)(void *,void **);
    const char *(*field_get_name)(const void *);
    uint32_t (*field_get_flags)(const void *);
    const void *(*field_get_type)(const void *);
    void (*field_get_value)(void *,const void *,void *);
    const void *(*method_get_param)(const void *,uint32_t);
    void *(*method_get_class)(const void *);
    bool (*type_is_byref)(const void *);
    void *(*type_get_object)(const void *);
    const uint16_t *(*string_chars)(void *);
} ReadApi;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
typedef struct {void *klass;const void *member;uint32_t kind,static_call;char name[129];} ReadCacheEntry;
typedef struct {void *klass;const void *member;char space[193],name[129];} ReadTypeCacheEntry;
#endif
typedef struct {
    uint32_t magic,owner,closed;atomic_uint gate;
    void *managed,*domain,*thread;
    SilphManagedApi base;
    ReadApi api;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    ReadCacheEntry fields[128],getters[128];
    ReadTypeCacheEntry typed_fields[32];
    SilphReadCacheMetrics cache;
#endif
#ifdef SILPH_ENABLE_ACTIONS
    void *(*object_new)(void *);
    void (*field_set_value)(void *,const void *,void *);
    void (*field_set_value_object)(void *,const void *,void *);
    void *(*method_get_object)(const void *,void *);
    const void *(*method_get_from_reflection)(void *);
    void *(*array_new)(void *,uintptr_t);
    uintptr_t (*array_length)(void *);
    void *(*research_nested)(void *,void **);
    int32_t (*research_value_size)(void *,uint32_t *);
    SilphManagedRef rpc_roots[SILPH_RPC_COUNT];
    const void *rpc_methods[SILPH_RPC_COUNT];
    void *rpc_classes[SILPH_RPC_COUNT];
#endif
} Read;
size_t silph_read_size(void) { return sizeof(Read); }
static void clear_value(SilphReadValue *value) { memset(value,0,sizeof(*value)); }
static int name_valid(const char *text,size_t maximum) { return text&&*text&&strnlen(text,maximum+1)<=maximum; }
static int enter_read(Read *state) {
    if(!state||state->magic!=READ_MAGIC)return SILPH_MANAGED_STATE;
    unsigned expected=0;
    if(!atomic_compare_exchange_strong_explicit(&state->gate,&expected,1,memory_order_acquire,memory_order_relaxed))return SILPH_MANAGED_BUSY;
    int status=0;
    if(state->closed)status=SILPH_MANAGED_STATE;
    else if(state->owner!=(uint32_t)syscall(__NR_gettid))status=SILPH_MANAGED_THREAD;
    else if(state->base.domain_get()!=state->domain||state->base.thread_current()!=state->thread)status=SILPH_MANAGED_STALE;
    else {
        SilphManagedMetrics metrics;
        status=silph_managed_metrics(state->managed,&metrics);
        if(!status&&metrics.closed)status=SILPH_MANAGED_STATE;
    }
    if(status)atomic_store_explicit(&state->gate,0,memory_order_release);
    return status;
}
static int leave_read(Read *state,int status) { atomic_store_explicit(&state->gate,0,memory_order_release);return status; }
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
static unsigned cache_index(void *klass,const char *name,uint32_t kind,uint32_t static_call){
    uint64_t hash=((uintptr_t)klass>>3)^kind^((uint64_t)static_call<<32);
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)hash=(hash^*p)*UINT64_C(1099511628211);
    return hash&127;
}
static const void *cache_find(Read *state,ReadCacheEntry *entry,void *klass,const char *name,uint32_t kind,uint32_t static_call){
    if(entry->member&&entry->klass==klass&&entry->kind==kind&&entry->static_call==static_call&&!strcmp(entry->name,name)){state->cache.hits++;return entry->member;}
    state->cache.misses++;return NULL;
}
static void cache_put(ReadCacheEntry *entry,void *klass,const char *name,uint32_t kind,uint32_t static_call,const void *member){
    *entry=(ReadCacheEntry){.klass=klass,.member=member,.kind=kind,.static_call=static_call};memcpy(entry->name,name,strlen(name)+1);
}
int silph_read_cache_metrics(void *memory,SilphReadCacheMetrics *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;Read *state=memory;int status=enter_read(state);if(status)return status;*out=state->cache;return leave_read(state,0);
}
#endif
int silph_read_open(void *memory,size_t bytes,void *managed,const SilphManagedApi *api,SilphManagedResolver resolve,void *context) {
    if(!memory||bytes!=sizeof(Read)||(uintptr_t)memory%_Alignof(Read)||!managed||!api||!resolve)return SILPH_MANAGED_ARGUMENT;
    uint32_t magic;memcpy(&magic,memory,sizeof(magic));if(magic==READ_MAGIC)return SILPH_MANAGED_STATE;
    SilphManagedMetrics metrics;int status=silph_managed_metrics(managed,&metrics);if(status||metrics.closed)return status?status:SILPH_MANAGED_STATE;
    ReadApi extra={0};
#define LOAD(name) do {void *symbol=resolve("il2cpp_" #name,context);if(!symbol)return SILPH_MANAGED_ABI_MISMATCH;memcpy(&extra.name,&symbol,sizeof(symbol));}while(0)
    LOAD(domain_get_assemblies);LOAD(assembly_get_image);LOAD(image_get_name);LOAD(class_from_name);LOAD(class_from_type);
    LOAD(class_get_name);LOAD(class_get_namespace);LOAD(class_get_parent);LOAD(class_is_valuetype);LOAD(class_is_enum);
    LOAD(class_is_assignable_from);LOAD(class_enum_basetype);LOAD(class_get_fields);LOAD(field_get_name);LOAD(field_get_flags);
    LOAD(field_get_type);LOAD(field_get_value);LOAD(method_get_param);LOAD(method_get_class);LOAD(type_is_byref);LOAD(type_get_object);LOAD(string_chars);
#undef LOAD
    memset(memory,0,bytes);Read *state=memory;atomic_init(&state->gate,0);
    state->owner=(uint32_t)syscall(__NR_gettid);state->managed=managed;state->base=*api;state->api=extra;
    state->domain=api->domain_get();state->thread=api->thread_current();state->magic=READ_MAGIC;return 0;
}
static int matches_kind(Read *state,const void *type,uint32_t kind) {
    if(!type||state->api.type_is_byref(type))return 0;
    int actual=state->base.type_get_type(type);void *klass=state->api.class_from_type(type);
    if(!klass)return 0;
    if(kind==SILPH_READ_VOID)return actual==1;
    if(kind==SILPH_READ_BOXED)return state->api.class_is_valuetype(klass)&&!state->api.class_is_enum(klass)&&(actual==17||actual==21);
    if(kind==SILPH_READ_REFERENCE)return !state->api.class_is_valuetype(klass)
        &&(actual==14||actual==18||actual==21||actual==28||actual==29||actual==20);
    if(kind==SILPH_READ_ENUM32) {
        const void *base=state->api.class_is_enum(klass)?state->api.class_enum_basetype(klass):NULL;
        return base&&state->base.type_get_type(base)==8;
    }
    return (kind==2||kind==8||kind==10||kind==11||kind==12||kind==13)&&actual==(int)kind;
}
static size_t scalar_bytes(uint32_t kind) { return kind==2?1:kind==8||kind==12||kind==SILPH_READ_ENUM32?4:8; }
int silph_read_is_type(void *memory,SilphManagedRef ref,const char *space,const char *name) {
    if(!space||strnlen(space,193)>192||!name_valid(name,128))return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL;status=silph_managed_target(state->managed,ref,&object);
    void *klass=status?NULL:state->base.object_get_class(object);
    const char *actual_space=klass?state->api.class_get_namespace(klass):NULL,*actual_name=klass?state->api.class_get_name(klass):NULL;
    if(!status&&(!actual_space||!actual_name||strcmp(space,actual_space)||strcmp(name,actual_name)))status=SILPH_MANAGED_CONTRACT;
    return leave_read(state,status);
}
static int accept_reference(Read *state,const void *type,void *object,SilphReadValue *output) {
    if(!object)return 0;
    int status=silph_managed_retain(state->managed,object,false,&output->reference);if(status)return status;
    void *held=NULL;status=silph_managed_target(state->managed,output->reference,&held);
    void *expected=state->api.class_from_type(type),*actual=status?NULL:state->base.object_get_class(held);
    if(!status&&(!expected||!actual||!state->api.class_is_assignable_from(expected,actual)))status=SILPH_MANAGED_CONTRACT;
    if(status){silph_managed_release(state->managed,output->reference);clear_value(output);}return status;
}
static const void *find_field(Read *state,void *klass,const char *name,const char *space,const char *type,int *status) {
    const void *found=NULL;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    void *actual_class=klass;ReadCacheEntry *cached=NULL;ReadTypeCacheEntry *typed=NULL;
    if(name&&name_valid(name,128)){cached=&state->fields[cache_index(klass,name,0,0)];found=cache_find(state,cached,klass,name,0,0);if(found)return found;}
    else if(!name&&space&&strnlen(space,193)<=192&&name_valid(type,128)){
        typed=&state->typed_fields[cache_index(klass,type,0,0)&31];
        if(typed->member&&typed->klass==klass&&!strcmp(typed->space,space)&&!strcmp(typed->name,type)){state->cache.hits++;return typed->member;}
        state->cache.misses++;
    }
#endif
    for(unsigned depth=0;klass&&depth<32;depth++,klass=state->api.class_get_parent(klass)) {
        void *iterator=NULL;
        for(unsigned count=0;;count++) {
            if(count>=2048){*status=SILPH_MANAGED_CONTRACT;return NULL;}
            const void *field=state->api.class_get_fields(klass,&iterator);if(!field)break;
            if(state->api.field_get_flags(field)&(0x10u|0x40u))continue;
            int match=0;
            if(name){const char *actual=state->api.field_get_name(field);match=actual&&name_valid(actual,128)&&!strcmp(actual,name);}
            else {
                const void *declared=state->api.field_get_type(field);
                void *value=declared?state->api.class_from_type(declared):NULL;
                const char *ns=value?state->api.class_get_namespace(value):NULL,*nm=value?state->api.class_get_name(value):NULL;
                match=ns&&nm&&!strcmp(ns,space)&&!strcmp(nm,type)&&matches_kind(state,declared,SILPH_READ_REFERENCE);
            }
            if(match){if(found){*status=SILPH_MANAGED_CONTRACT;return NULL;}found=field;}
        }
        if(depth==31&&state->api.class_get_parent(klass)){*status=SILPH_MANAGED_CONTRACT;return NULL;}
    }
    if(!found)*status=SILPH_MANAGED_CONTRACT;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    if(found&&!*status&&cached)cache_put(cached,actual_class,name,0,0,found);
    if(found&&!*status&&typed){*typed=(ReadTypeCacheEntry){.klass=actual_class,.member=found};memcpy(typed->space,space,strlen(space)+1);memcpy(typed->name,type,strlen(type)+1);}
#endif
    return found;
}
int silph_read_field(void *memory,SilphManagedRef ref,const char *name,uint32_t kind,SilphReadValue *output) {
    if(!output)return SILPH_MANAGED_ARGUMENT;clear_value(output);
    if(!name_valid(name,128)||kind==SILPH_READ_BOXED||kind==SILPH_READ_VOID)return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL;status=silph_managed_target(state->managed,ref,&object);
    const void *field=status?NULL:find_field(state,state->base.object_get_class(object),name,NULL,NULL,&status);
    const void *type=field?state->api.field_get_type(field):NULL;
    if(!status&&!matches_kind(state,type,kind))status=SILPH_MANAGED_CONTRACT;
    if(!status) {
        if(kind==SILPH_READ_REFERENCE){void *value=NULL;state->api.field_get_value(object,field,&value);status=accept_reference(state,type,value,output);}
        else {state->api.field_get_value(object,field,&output->bits);if(kind==2&&output->bits>1)status=SILPH_MANAGED_CONTRACT;}
    }
    if(status)clear_value(output);return leave_read(state,status);
}
int silph_read_field_type(void *memory,SilphManagedRef ref,const char *space,const char *name,SilphManagedRef *output) {
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    if(!space||strnlen(space,193)>192||!name_valid(name,128))return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL;status=silph_managed_target(state->managed,ref,&object);
    const void *field=status?NULL:find_field(state,state->base.object_get_class(object),NULL,space,name,&status);
    if(!status) {
        void *value=NULL;state->api.field_get_value(object,field,&value);SilphReadValue result={0};
        status=accept_reference(state,state->api.field_get_type(field),value,&result);*output=result.reference;
    }
    return leave_read(state,status);
}
static int arguments_match(Read *state,const void *method,const SilphReadArgument *args,uint32_t count,bool boxed) {
    for(uint32_t i=0;i<count;i++) {
        const void *type=state->api.method_get_param(method,i);
        if(args[i].reserved||(!boxed&&args[i].kind==SILPH_READ_BOXED)||args[i].kind==SILPH_READ_VOID||!matches_kind(state,type,args[i].kind))return 0;
        if(args[i].kind==SILPH_READ_REFERENCE||args[i].kind==SILPH_READ_BOXED) {
            void *object=NULL;
            if(silph_managed_target(state->managed,args[i].value.reference,&object))return 0;
            if(!state->api.class_is_assignable_from(state->api.class_from_type(type),state->base.object_get_class(object)))return 0;
        }else if(args[i].kind==2&&args[i].value.bits>1)return 0;
    }
    return 1;
}
static bool local_geometry(Read *s,const void *method){
    void *klass=s->api.method_get_class(method);const char *space=klass?s->api.class_get_namespace(klass):NULL;
    const char *name=klass?s->api.class_get_name(klass):NULL,*member=s->base.method_get_name(method);
    return space&&name&&member&&!strcmp(space,"Google.Common.Geometry")&&!strcmp(name,"S2CellId")&&!strcmp(member,"FromLatLng");
}
static int invoke_checked(Read *state,const void *found,void *object,uint32_t kind,const SilphReadArgument *args,
    uint32_t count,SilphReadValue *output,const void *permit) {
    int status=0;
    if(!found||count>5||state->base.method_get_param_count(found)!=count
        ||!matches_kind(state,state->base.method_get_return_type(found),kind)||!arguments_match(state,found,args,count,permit!=NULL||local_geometry(state,found)))return SILPH_MANAGED_CONTRACT;
    if(!status) {
        void *parameters[5]={0};uint64_t scalars[5]={0};
        for(uint32_t i=0;i<count;i++) {
            if(args[i].kind==SILPH_READ_REFERENCE||args[i].kind==SILPH_READ_BOXED) {
                status=silph_managed_target(state->managed,args[i].value.reference,&parameters[i]);
                if(!status&&args[i].kind==SILPH_READ_BOXED){parameters[i]=state->base.object_unbox(parameters[i]);if(!parameters[i])status=SILPH_MANAGED_CONTRACT;}
            }
            else {scalars[i]=args[i].value.bits;parameters[i]=&scalars[i];}
            if(status)break;
        }
        void *instance=object;
        if(!status&&object&&state->api.class_is_valuetype(state->api.method_get_class(found))) {
            instance=state->base.object_unbox(object);if(!instance)status=SILPH_MANAGED_CONTRACT;
        }
#ifdef SILPH_ENABLE_ACTIONS
        if(!status&&permit&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
#else
        if(permit)status=SILPH_MANAGED_STATE;
#endif
        if(!status) {
            void *exception=NULL,*result=state->base.runtime_invoke(found,instance,count?parameters:NULL,&exception);
            if(exception){status=SILPH_MANAGED_EXCEPTION;
#ifdef SILPH_PRESENT_SCOPED
                const char *called=state->base.method_get_name(found);if(called&&!strcmp(called,"TapTappable")){
                    void *klass=state->base.object_get_class(exception),*message=NULL;int field_status=0;const void *field=find_field(state,klass,"_message",NULL,NULL,&field_status);char text[257]={0};
                    if(!field_status&&field)state->api.field_get_value(exception,field,&message);
                    if(message){int32_t length=state->base.string_length(message);const uint16_t *chars=state->api.string_chars(message);if(chars&&length>=0&&length<4096)for(int32_t i=0;i<length&&i<256;i++)text[i]=chars[i]>=32&&chars[i]<127?(char)chars[i]:'?';}
                    __android_log_print(ANDROID_LOG_INFO,"SilphEvent","interaction exception type=%s message=%s",state->api.class_get_name(klass),text);
                }
#endif
            }
            else if(kind==SILPH_READ_VOID){if(result)status=SILPH_MANAGED_CONTRACT;}
            else if(kind==SILPH_READ_REFERENCE||kind==SILPH_READ_BOXED)status=accept_reference(state,state->base.method_get_return_type(found),result,output);
            else if(!result)status=SILPH_MANAGED_CONTRACT;
            else {
                SilphManagedRef held={0};status=silph_managed_retain(state->managed,result,false,&held);
                if(!status) {
                    void *value=NULL;status=silph_managed_target(state->managed,held,&value);
                    if(!status&&state->base.object_get_class(value)!=state->api.class_from_type(state->base.method_get_return_type(found)))status=SILPH_MANAGED_CONTRACT;
                    void *scalar=status?NULL:state->base.object_unbox(value);
                    if(!status&&!scalar)status=SILPH_MANAGED_CONTRACT;
                    if(!status){memcpy(&output->bits,scalar,scalar_bytes(kind));if(kind==2&&output->bits>1)status=SILPH_MANAGED_CONTRACT;}
                    int released=silph_managed_release(state->managed,held);if(!status)status=released;
                }
            }
        }
    }
    if(status)clear_value(output);return status;
}
static int call_internal(Read *state,SilphManagedRef ref,void *static_class,const char *name,uint32_t kind,
    const SilphReadArgument *args,uint32_t count,SilphReadValue *output,const void *permit) {
    if(count>5||(count&&!args))return SILPH_MANAGED_ARGUMENT;
    int status=0;void *object=NULL;
    if(!static_class)status=silph_managed_target(state->managed,ref,&object);
    void *klass=static_class?static_class:status?NULL:state->base.object_get_class(object);const void *found=NULL;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    void *actual_class=klass;ReadCacheEntry *cached=NULL;bool cache_store=false;
    if(!status&&!count&&name_valid(name,128)){cached=&state->getters[cache_index(klass,name,kind,static_class!=NULL)];found=cache_find(state,cached,klass,name,kind,static_class!=NULL);cache_store=found==NULL;}
#endif
    for(unsigned depth=0;!status&&!found&&klass&&depth<32;depth++,klass=state->api.class_get_parent(klass)) {
        void *iterator=NULL;
        for(unsigned i=0;;i++) {
            if(i>=2048){status=SILPH_MANAGED_CONTRACT;break;}
            const void *method=state->base.class_get_methods(klass,&iterator);if(!method)break;
            const char *actual=state->base.method_get_name(method);uint32_t flags=0;
            if(!actual||strcmp(actual,name)||state->base.method_get_param_count(method)!=count)continue;
            if(((state->base.method_get_flags(method,&flags)&0x10)!=0)!=(static_class!=NULL)||state->base.method_is_generic(method)
                ||!matches_kind(state,state->base.method_get_return_type(method),kind)||!arguments_match(state,method,args,count,permit!=NULL||local_geometry(state,method)))continue;
            if(found){status=SILPH_MANAGED_CONTRACT;break;}found=method;
        }
    }
    if(!status&&!found)status=SILPH_MANAGED_CONTRACT;
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    if(!status&&cache_store&&found)cache_put(cached,actual_class,name,kind,static_class!=NULL,found);
#endif
    if(!status)status=invoke_checked(state,found,object,kind,args,count,output,permit);
    return status;
}
int silph_read_call(void *memory,SilphManagedRef ref,const char *name,uint32_t kind,const SilphReadArgument *args,uint32_t count,SilphReadValue *output) {
    if(!output)return SILPH_MANAGED_ARGUMENT;clear_value(output);
    if(!name_valid(name,128)||count>2||(count&&!args)
        ||(strncmp(name,"get_",4)&&strcmp(name,"GetItemCount")&&strcmp(name,"GetTotalItems")&&strcmp(name,"TryResolve")
            &&strcmp(name,"GetEnumerator")&&strcmp(name,"MoveNext")&&strcmp(name,"Dispose")
            &&strcmp(name,"GetPokemon")&&strcmp(name,"GetIsBuddy")&&strcmp(name,"GetClass"))
        ||(kind==SILPH_READ_VOID&&strcmp(name,"Dispose")))return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    status=call_internal(state,ref,NULL,name,kind,args,count,output,NULL);return leave_read(state,status);
}
static void *lookup_class(Read *state,const char *assembly,const char *space,const char *name) {
    size_t count=0;const void **assemblies=state->api.domain_get_assemblies(state->domain,&count);const void *image=NULL;
    if(!assemblies||!count||count>2048)return NULL;
    for(size_t i=0;i<count;i++) {
        const void *candidate=assemblies[i]?state->api.assembly_get_image(assemblies[i]):NULL;
        const char *actual=candidate?state->api.image_get_name(candidate):NULL;
        if(actual&&!strcmp(actual,assembly)){if(image)return NULL;image=candidate;}
    }
    return image?state->api.class_from_name(image,space,name):NULL;
}
int silph_read_type_object(void *memory,const char *assembly,const char *space,const char *name,SilphManagedRef *output) {
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    if(!name_valid(assembly,128)||!space||strnlen(space,193)>192||!name_valid(name,128))return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *klass=lookup_class(state,assembly,space,name);
    const void *type=klass?state->base.class_get_type(klass):NULL;void *value=type?state->api.type_get_object(type):NULL;
    if(!status&&!value)status=SILPH_MANAGED_CONTRACT;
    if(!status)status=silph_managed_retain(state->managed,value,false,output);
    return leave_read(state,status);
}
int silph_read_utf8(void *memory,SilphManagedRef ref,char *output,uint32_t capacity,uint32_t *length) {
    if(!output||!capacity||capacity>8192||!length)return SILPH_MANAGED_ARGUMENT;output[0]=0;*length=0;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL;status=silph_managed_target(state->managed,ref,&object);
    void *klass=status?NULL:state->base.object_get_class(object);
    if(!status&&state->base.type_get_type(state->base.class_get_type(klass))!=14)status=SILPH_MANAGED_CONTRACT;
    int32_t count=status?0:state->base.string_length(object);const uint16_t *text=status?NULL:state->api.string_chars(object);
    if(!status&&(count<0||count>2048||!text))status=SILPH_MANAGED_CONTRACT;
    uint32_t written=0;
    for(int32_t i=0;!status&&i<count;i++) {
        uint32_t code=text[i];
        if(code>=0xd800&&code<=0xdbff) {
            if(i+1>=count||text[i+1]<0xdc00||text[i+1]>0xdfff){status=SILPH_MANAGED_CONTRACT;break;}
            code=0x10000+((code-0xd800)<<10)+(text[++i]-0xdc00);
        }else if(code>=0xdc00&&code<=0xdfff){status=SILPH_MANAGED_CONTRACT;break;}
        if(!code){status=SILPH_MANAGED_CONTRACT;break;}
        unsigned bytes=code<0x80?1:code<0x800?2:code<0x10000?3:4;
        if(written+bytes>=capacity){status=SILPH_MANAGED_CAPACITY;break;}
        if(bytes==1)output[written++]=(char)code;
        else {
            output[written++]=(char)((bytes==2?0xc0:bytes==3?0xe0:0xf0)|(code>>(6*(bytes-1))));
            for(unsigned j=bytes-1;j>0;j--)output[written++]=(char)(0x80|((code>>(6*(j-1)))&0x3f));
        }
    }
    if(!status){output[written]=0;*length=written;}else memset(output,0,capacity);
    return leave_read(state,status);
}
int silph_read_close(void *memory) {
    Read *state=memory;int status=enter_read(state);if(status)return status;
#ifdef SILPH_ENABLE_ACTIONS
    for(unsigned i=0;i<SILPH_RPC_COUNT;i++)if(state->rpc_roots[i].serial){
        status=silph_managed_release(state->managed,state->rpc_roots[i]);if(status)return leave_read(state,status);
        memset(&state->rpc_roots[i],0,sizeof(state->rpc_roots[i]));state->rpc_methods[i]=NULL;state->rpc_classes[i]=NULL;
    }
#endif
    state->closed=1;memset(&state->api,0,sizeof(state->api));memset(&state->base,0,sizeof(state->base));state->managed=NULL;
    return leave_read(state,0);
}
#ifdef SILPH_ENABLE_ACTIONS
#include "managed-rpc.inc"
#include "managed-proto.inc"
#include "research-bridge.inc"
#ifdef SILPH_PRESENT_SCOPED
#include "event-actions.h"
int silph_event_tap(void *memory,const SilphSchedulePermit *permit,SilphManagedRef service,SilphManagedRef tappable){
    if(!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;Read *s=memory;int r=enter_read(s);if(r)return r;void *a=NULL,*b=NULL;
    r=silph_managed_target(s->managed,service,&a);if(!r)r=silph_managed_target(s->managed,tappable,&b);
    if(!r&&(!research_type(s,a,"Niantic.Holoholo.Services.DynamicTappables","DynamicTappablesSession")||!research_type(s,b,"","DynamicTappableController")))r=SILPH_MANAGED_CONTRACT;
    SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=tappable};SilphReadValue out={0};
    if(!r)r=call_internal(s,service,NULL,"TapTappable",SILPH_READ_VOID,&arg,1,&out,permit);int d=release_rpc_ref(s,&out.reference);return leave_read(s,r?r:d);
}
#endif
#include "gift-bridge.inc"
#include "pass-bridge.inc"
#include "rocket-bridge.inc"
int silph_action_enable(void *memory,SilphManagedResolver resolve,void *context) {
    if(!resolve)return SILPH_MANAGED_ARGUMENT;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *create=resolve("il2cpp_object_new",context),*set=resolve("il2cpp_field_set_value",context);
    if(!create||!set)status=SILPH_MANAGED_ABI_MISMATCH;
    if(!status){memcpy(&state->object_new,&create,sizeof(create));memcpy(&state->field_set_value,&set,sizeof(set));}
    return leave_read(state,status);
}
int silph_action_call(void *memory,const SilphSchedulePermit *permit,uint32_t operation,SilphManagedRef ref,
    const SilphReadArgument *args,uint32_t count,SilphReadValue *output) {
    if(!output)return SILPH_MANAGED_ARGUMENT;clear_value(output);
    const char *name=operation==SILPH_ACTION_ENCOUNTER?"SendEncounterRequest":operation==SILPH_ACTION_BERRY?"UseItemForEncounter":operation==SILPH_ACTION_CAPTURE?"TryCapture":NULL;
    if(!name||count!=(operation==SILPH_ACTION_ENCOUNTER?0u:2u)||(count&&!args))return SILPH_MANAGED_ARGUMENT;
    if(!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL;status=silph_managed_target(state->managed,ref,&object);
    void *expected=lookup_class(state,"holo-game.dll",operation==SILPH_ACTION_BERRY?"Niantic.Holoholo":"Niantic.Holoholo.Map",
        operation==SILPH_ACTION_BERRY?"IItemBag":"MapPokemon");
    if(!status&&(!expected||(!state->api.class_is_assignable_from(expected,state->base.object_get_class(object))
        &&!(operation==SILPH_ACTION_CAPTURE&&research_type(state,object,"Niantic.Holoholo.Quests","QuestMapPokemon")))))status=SILPH_MANAGED_CONTRACT;
    if(!status)status=call_internal(state,ref,NULL,name,SILPH_READ_REFERENCE,args,count,output,permit);
    return leave_read(state,status);
}
static int set_scalar(Read *state,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t bits,const SilphSchedulePermit *permit) {
    void *object=NULL;int status=silph_managed_target(state->managed,ref,&object);
    const void *field=status?NULL:find_field(state,state->base.object_get_class(object),name,NULL,NULL,&status);
    if(!status&&!matches_kind(state,state->api.field_get_type(field),kind))status=SILPH_MANAGED_CONTRACT;
    if(!status&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
    if(!status)state->field_set_value(object,field,&bits);return status;
}
int silph_action_capture_data(void *memory,const SilphSchedulePermit *permit,int32_t ball,float reticle,bool bullseye,bool curve,
    SilphManagedRef *throw_data,SilphManagedRef *ar_data) {
    if(!throw_data||!ar_data||throw_data==ar_data)return SILPH_MANAGED_ARGUMENT;
    memset(throw_data,0,sizeof(*throw_data));memset(ar_data,0,sizeof(*ar_data));
    if(ball<1||ball>3||!isfinite(reticle)||reticle<0||reticle>1.1f)return SILPH_MANAGED_ARGUMENT;
    if(!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    if(!state->object_new||!state->field_set_value)status=SILPH_MANAGED_STATE;
    void *throw_class=lookup_class(state,"holo-game.dll","Niantic.Holoholo.Encounter","PokeballThrow");
    void *ar_class=lookup_class(state,"holo-protos.dll","Holoholo.Rpc","ARPlusEncounterValuesProto");
    if(!status&&(!throw_class||!ar_class||!state->api.class_is_valuetype(throw_class)||state->api.class_is_valuetype(ar_class)))status=SILPH_MANAGED_CONTRACT;
    SilphReadValue value={0};SilphReadArgument argument={.kind=SILPH_READ_ENUM32,.value.bits=(uint32_t)ball};
    if(!status)status=call_internal(state,(SilphManagedRef){0},throw_class,"MissedThrow",SILPH_READ_BOXED,&argument,1,&value,permit);
    if(!status)*throw_data=value.reference;
    uint32_t bits=0;memcpy(&bits,&reticle,sizeof(bits));
    if(!status)status=set_scalar(state,*throw_data,"ReticleSize",SILPH_READ_F32,bits,permit);
    if(!status)status=set_scalar(state,*throw_data,"HitBullseye",SILPH_READ_BOOL,bullseye,permit);
    if(!status)status=set_scalar(state,*throw_data,"Spinning",SILPH_READ_BOOL,curve,permit);
    if(!status)status=set_scalar(state,*throw_data,"Missed",SILPH_READ_BOOL,0,permit);
    if(!status&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
    void *created=status?NULL:state->object_new(ar_class);
    if(!status&&!created)status=SILPH_MANAGED_CAPACITY;
    if(!status)status=silph_managed_retain(state->managed,created,false,ar_data);
    if(!status)status=call_internal(state,*ar_data,NULL,".ctor",SILPH_READ_VOID,NULL,0,&value,permit);
    if(status){if(throw_data->serial)silph_managed_release(state->managed,*throw_data);if(ar_data->serial)silph_managed_release(state->managed,*ar_data);
        memset(throw_data,0,sizeof(*throw_data));memset(ar_data,0,sizeof(*ar_data));}
    return leave_read(state,status);
}
int silph_action_release(void *memory,const SilphSchedulePermit *permit,void *scope,uint64_t encounter,
    const SilphTransferInput *input,SilphManagedRef bag,SilphManagedRef pokemon,SilphReadValue *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;clear_value(output);SilphTransferVerdict verdict;
    if(!input||silph_transfer_evaluate(input,sizeof(*input),&verdict)||verdict.decision!=SILPH_VERDICT_CANDIDATE
        ||!silph_core_authorize(scope,input->owned_id,encounter)||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *state=memory;int status=enter_read(state);if(status)return status;
    void *object=NULL,*bag_object=NULL;status=silph_managed_target(state->managed,pokemon,&object);
    if(!status)status=silph_managed_target(state->managed,bag,&bag_object);
    void *expected=lookup_class(state,"holo-protos.dll","Holoholo.Rpc","PokemonProto");
    void *bag_class=lookup_class(state,"holo-game.dll","Niantic.Holoholo","IPokemonBag");
    if(!status&&(!expected||!bag_class||state->base.object_get_class(object)!=expected
        ||!state->api.class_is_assignable_from(bag_class,state->base.object_get_class(bag_object))))status=SILPH_MANAGED_CONTRACT;
    const void *field=status?NULL:find_field(state,expected,"id_",NULL,NULL,&status);uint64_t id=0;
    if(!status&&!matches_kind(state,state->api.field_get_type(field),SILPH_READ_U64))status=SILPH_MANAGED_CONTRACT;
    if(!status){state->api.field_get_value(object,field,&id);if(id!=input->owned_id)status=SILPH_MANAGED_STALE;}
    if(!status&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
    if(!status&&silph_core_begin(scope,id,encounter))status=SILPH_MANAGED_STATE;
    SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=pokemon};
    if(!status)status=call_internal(state,bag,NULL,"ReleasePokemon",SILPH_READ_REFERENCE,&arg,1,output,permit);
    return leave_read(state,status);
}
#endif
