#define _GNU_SOURCE
#include "managed-promise.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC UINT32_C(0x534d5031)
typedef struct {
    uint32_t magic,rpc;
    void *read,*managed,*tracker;
    SilphManagedRef promise,result;
    SilphPromiseTicket ticket;
    char space[193],name[129];
} ManagedPromise;
size_t silph_managed_promise_size(void){return sizeof(ManagedPromise);}
static int release(ManagedPromise *s,SilphManagedRef *ref){
    if(!ref->serial)return 0;int status=silph_managed_release(s->managed,*ref);if(!status)memset(ref,0,sizeof(*ref));return status;
}
int silph_managed_promise_poll(void *memory,const SilphScheduleIdentity *identity,uint64_t now,SilphPromiseMetrics *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));ManagedPromise *s=memory;
    if(!s||s->magic!=MAGIC)return SILPH_MANAGED_STATE;
    int checked=silph_promise_check(s->tracker,s->ticket,identity,now);
    int snapshot=silph_promise_metrics(s->tracker,s->ticket,output);
    if(snapshot)return SILPH_MANAGED_STATE;
    if(checked||output->state!=SILPH_PROMISE_WAITING)return 0;
    SilphReadValue complete={0},error={0},rpc_error={0},result={0};
    int status=silph_read_field(s->read,s->promise,"completeCalled",SILPH_READ_BOOL,&complete);
    if(!status)status=silph_read_field(s->read,s->promise,"errorCalled",SILPH_READ_BOOL,&error);
    if(!status&&s->rpc)status=silph_read_field(s->read,s->promise,"statusErrorCalled",SILPH_READ_BOOL,&rpc_error);
    if(!status&&rpc_error.bits)error.bits=1;
    if(!status&&complete.bits&&!error.bits)status=silph_read_field(s->read,s->promise,"completedValue",SILPH_READ_REFERENCE,&result);
    if(!status&&complete.bits&&!error.bits&&result.reference.serial)status=silph_read_is_type(s->read,result.reference,s->space,s->name);
    int observed=silph_promise_observe(s->tracker,s->ticket,identity,now,status?2u:(uint32_t)complete.bits,
        (uint32_t)error.bits,result.reference.serial?1u:0u);
    if(observed&&!status)status=SILPH_MANAGED_STATE;
    snapshot=silph_promise_metrics(s->tracker,s->ticket,output);if(snapshot&&!status)status=SILPH_MANAGED_STATE;
    if(!status&&output->state==SILPH_PROMISE_RESOLVED){s->result=result.reference;memset(&result.reference,0,sizeof(result.reference));}
    int freed=release(s,&result.reference);return status?status:freed;
}
static int open_promise(void *memory,size_t bytes,void *read,void *managed,SilphManagedRef promise,
    const SilphScheduleIdentity *identity,uint64_t action,uint64_t encounter,uint64_t now,const char *space,const char *name,unsigned rpc){
    if(!memory||bytes!=sizeof(ManagedPromise)||(uintptr_t)memory%_Alignof(ManagedPromise)||!read||!managed||!promise.serial
        ||!space||strnlen(space,193)>192||!name||!*name||strnlen(name,129)>128)return SILPH_MANAGED_ARGUMENT;
    ManagedPromise *s=memory;if(s->magic==MAGIC)return SILPH_MANAGED_STATE;
    void *object=NULL;int status=silph_managed_target(managed,promise,&object);if(status)return status;
    memset(s,0,bytes);s->read=read;s->managed=managed;s->tracker=calloc(1,silph_promise_size());
    if(!s->tracker)return SILPH_MANAGED_CAPACITY;
    if(silph_promise_init(s->tracker,silph_promise_size())
        ||silph_promise_begin(s->tracker,identity,action,encounter,now,UINT64_C(18000000000),&s->ticket)){
        free(s->tracker);memset(s,0,bytes);return SILPH_MANAGED_ARGUMENT;
    }
    status=silph_managed_retain(managed,object,false,&s->promise);
    if(status){free(s->tracker);memset(s,0,bytes);return status;}
    memcpy(s->space,space,strlen(space)+1);memcpy(s->name,name,strlen(name)+1);s->magic=MAGIC;s->rpc=rpc;
    SilphPromiseMetrics metrics;status=silph_managed_promise_poll(s,identity,now,&metrics);
    if(status)silph_managed_promise_close(s);
    return status;
}
int silph_managed_promise_open(void *memory,size_t bytes,void *read,void *managed,SilphManagedRef promise,
    const SilphScheduleIdentity *identity,uint64_t action,uint64_t encounter,uint64_t now,const char *space,const char *name){
    return open_promise(memory,bytes,read,managed,promise,identity,action,encounter,now,space,name,0);
}
int silph_managed_promise_open_rpc(void *memory,size_t bytes,void *read,void *managed,SilphManagedRef promise,
    const SilphScheduleIdentity *identity,uint64_t action,uint64_t encounter,uint64_t now,const char *space,const char *name){
    return open_promise(memory,bytes,read,managed,promise,identity,action,encounter,now,space,name,1);
}
int silph_managed_promise_take(void *memory,const SilphScheduleIdentity *identity,uint64_t now,SilphManagedRef *output,bool *fresh){
    if(!output||!fresh)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));*fresh=false;ManagedPromise *s=memory;
    if(!s||s->magic!=MAGIC||!s->result.serial)return SILPH_MANAGED_STATE;
    if(silph_promise_consume(s->tracker,s->ticket,identity,now,fresh))return SILPH_MANAGED_STATE;
    *output=s->result;memset(&s->result,0,sizeof(s->result));return 0;
}
int silph_managed_promise_close(void *memory){
    ManagedPromise *s=memory;if(!s)return SILPH_MANAGED_ARGUMENT;if(!s->magic)return 0;
    if(s->magic!=MAGIC)return SILPH_MANAGED_STATE;
    SilphPromiseMetrics metrics;if(silph_promise_metrics(s->tracker,s->ticket,&metrics))return SILPH_MANAGED_THREAD;
    if(metrics.state==SILPH_PROMISE_WAITING||metrics.state==SILPH_PROMISE_RESOLVED)silph_promise_cancel(s->tracker,s->ticket);
    int a=release(s,&s->result),b=release(s,&s->promise);if(a||b)return a?a:b;
    free(s->tracker);memset(s,0,sizeof(*s));return 0;
}
