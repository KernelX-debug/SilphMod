#define _GNU_SOURCE
#include "promise.h"
#include <stdatomic.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MAGIC UINT32_C(0x53504d31)
typedef struct { uint32_t magic;SilphScheduleIdentity identity;SilphPromiseMetrics metrics; } Promise;
static atomic_uint_fast64_t next_epoch=ATOMIC_VAR_INIT(1);
static uint32_t tid(void){return(uint32_t)syscall(__NR_gettid);}
static bool identity_valid(const SilphScheduleIdentity *id){
    return id&&id->pid&&!id->reserved&&id->process_ticks&&(id->session_hi||id->session_lo)&&(id->account_hi||id->account_lo);
}
static bool same_identity(const SilphScheduleIdentity *a,const SilphScheduleIdentity *b){
    return identity_valid(b)&&a->pid==b->pid&&a->process_ticks==b->process_ticks&&a->session_hi==b->session_hi
        &&a->session_lo==b->session_lo&&a->account_hi==b->account_hi&&a->account_lo==b->account_lo;
}
size_t silph_promise_size(void){return sizeof(Promise);}
int silph_promise_init(void *memory,size_t bytes){
    if(!memory||bytes!=sizeof(Promise)||(uintptr_t)memory%_Alignof(Promise))return SILPH_PROMISE_ARGUMENT;
    memset(memory,0,bytes);Promise *state=memory;state->magic=MAGIC;state->metrics.owner_tid=tid();return 0;
}
static int owned(Promise *state){
    if(!state||state->magic!=MAGIC)return SILPH_PROMISE_ARGUMENT;
    return state->metrics.owner_tid==tid()?0:SILPH_PROMISE_THREAD;
}
static int ticket_valid(Promise *state,SilphPromiseTicket ticket){
    int status=owned(state);if(status)return status;
    return ticket.epoch&&ticket.epoch==state->metrics.epoch&&ticket.action==state->metrics.action
        &&ticket.encounter==state->metrics.encounter?0:SILPH_PROMISE_STALE;
}
static void finish(Promise *state,unsigned status,unsigned reason){
    state->metrics.state=status;state->metrics.reason=reason;
    if(status!=SILPH_PROMISE_RESOLVED&&status!=SILPH_PROMISE_CONSUMED)state->metrics.observed_pending=0;
}
static bool timestamp(Promise *state,const SilphScheduleIdentity *identity,uint64_t now){
    if(!same_identity(&state->identity,identity)){finish(state,SILPH_PROMISE_CANCELED,SILPH_PROMISE_REASON_IDENTITY);return false;}
    if(!now||now<state->metrics.last_ns){finish(state,SILPH_PROMISE_UNCERTAIN,SILPH_PROMISE_REASON_CLOCK);return false;}
    state->metrics.last_ns=now;
    if(now>=state->metrics.deadline_ns){finish(state,SILPH_PROMISE_TIMED_OUT,SILPH_PROMISE_REASON_TIMEOUT);return false;}
    return true;
}
int silph_promise_begin(void *memory,const SilphScheduleIdentity *identity,uint64_t action,uint64_t encounter,
    uint64_t now,uint64_t timeout,SilphPromiseTicket *output){
    if(!output)return SILPH_PROMISE_ARGUMENT;memset(output,0,sizeof(*output));Promise *state=memory;
    int status=owned(state);if(status)return status;
    if(!identity_valid(identity)||!action||!encounter||!now||timeout<1000000||timeout>UINT64_C(30000000000)
        ||now>UINT64_MAX-timeout)return SILPH_PROMISE_ARGUMENT;
    if(state->metrics.state==SILPH_PROMISE_WAITING||state->metrics.state==SILPH_PROMISE_RESOLVED)return SILPH_PROMISE_STATE;
    if(state->metrics.epoch&&same_identity(&state->identity,identity)&&action<=state->metrics.action)return SILPH_PROMISE_STALE;
    uint_fast64_t epoch=atomic_load_explicit(&next_epoch,memory_order_relaxed);
    while(epoch&&epoch!=UINT64_MAX&&!atomic_compare_exchange_weak_explicit(&next_epoch,&epoch,epoch+1,memory_order_relaxed,memory_order_relaxed)){}
    if(!epoch||epoch==UINT64_MAX)return SILPH_PROMISE_STATE;
    state->identity=*identity;state->metrics=(SilphPromiseMetrics){.epoch=epoch,.action=action,.encounter=encounter,
        .started_ns=now,.last_ns=now,.deadline_ns=now+timeout,.state=SILPH_PROMISE_WAITING,.owner_tid=tid()};
    *output=(SilphPromiseTicket){epoch,action,encounter};return 0;
}
int silph_promise_check(void *memory,SilphPromiseTicket ticket,const SilphScheduleIdentity *identity,uint64_t now){
    Promise *state=memory;int status=ticket_valid(state,ticket);if(status)return status;
    if(state->metrics.state!=SILPH_PROMISE_WAITING&&state->metrics.state!=SILPH_PROMISE_RESOLVED)return SILPH_PROMISE_STATE;
    return timestamp(state,identity,now)?0:SILPH_PROMISE_STATE;
}
int silph_promise_observe(void *memory,SilphPromiseTicket ticket,const SilphScheduleIdentity *identity,uint64_t now,
    uint32_t complete,uint32_t error,uint32_t has_result){
    Promise *state=memory;int status=ticket_valid(state,ticket);if(status)return status;
    if(state->metrics.state!=SILPH_PROMISE_WAITING)return SILPH_PROMISE_STATE;
    if(!timestamp(state,identity,now))return 0;
    state->metrics.polls++;
    if(complete>1||error>1||has_result>1||(complete&&error)||(!complete&&has_result)||(complete&&!has_result))
        finish(state,SILPH_PROMISE_UNCERTAIN,SILPH_PROMISE_REASON_FLAGS);
    else if(error)finish(state,SILPH_PROMISE_FAILED,SILPH_PROMISE_REASON_NONE);
    else if(complete)finish(state,SILPH_PROMISE_RESOLVED,SILPH_PROMISE_REASON_NONE);
    else state->metrics.observed_pending=1;
    return 0;
}
int silph_promise_consume(void *memory,SilphPromiseTicket ticket,const SilphScheduleIdentity *identity,uint64_t now,bool *fresh){
    if(!fresh)return SILPH_PROMISE_ARGUMENT;*fresh=false;Promise *state=memory;int status=ticket_valid(state,ticket);if(status)return status;
    if(state->metrics.state!=SILPH_PROMISE_RESOLVED)return SILPH_PROMISE_STATE;
    if(!timestamp(state,identity,now))return SILPH_PROMISE_STATE;
    *fresh=state->metrics.observed_pending!=0;finish(state,SILPH_PROMISE_CONSUMED,SILPH_PROMISE_REASON_NONE);return 0;
}
int silph_promise_cancel(void *memory,SilphPromiseTicket ticket){
    Promise *state=memory;int status=ticket_valid(state,ticket);if(status)return status;
    if(state->metrics.state!=SILPH_PROMISE_WAITING&&state->metrics.state!=SILPH_PROMISE_RESOLVED)return SILPH_PROMISE_STATE;
    finish(state,SILPH_PROMISE_CANCELED,SILPH_PROMISE_REASON_CANCEL);return 0;
}
int silph_promise_metrics(void *memory,SilphPromiseTicket ticket,SilphPromiseMetrics *output){
    if(!output)return SILPH_PROMISE_ARGUMENT;memset(output,0,sizeof(*output));Promise *state=memory;
    int status=ticket_valid(state,ticket);if(status)return status;*output=state->metrics;return 0;
}
