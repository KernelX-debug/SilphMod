#define _GNU_SOURCE
#include "kecleon-plan.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
typedef struct {
    uint32_t opened,owner,maximum,used,capacity;
    uint64_t clock,deadline;
    SilphScheduleIdentity identity;
    SilphKecleonPlanState state;
    char (*history)[97];
} Plan;
size_t silph_kecleon_plan_size(void){return sizeof(Plan);}
static int identity_valid(const SilphScheduleIdentity *id){return id&&id->pid>1&&id->process_ticks&&(id->session_hi||id->session_lo)&&(id->account_hi||id->account_lo)&&!id->reserved;}
static int guard(Plan *p,const SilphScheduleIdentity *id,uint64_t now){
    if(!p||!p->opened)return SILPH_MANAGED_STATE;if(p->owner!=(uint32_t)syscall(__NR_gettid))return SILPH_MANAGED_THREAD;
    if(!identity_valid(id)||memcmp(&p->identity,id,sizeof(*id))){p->state.phase=SILPH_KPLAN_FINISHED;p->state.reason=SILPH_KPLAN_ACCOUNT;return SILPH_MANAGED_STALE;}
    if(!now||now<p->clock||now>UINT64_MAX-UINT64_C(180000000000))return SILPH_MANAGED_STATE;p->clock=now;return 0;
}
static void result(Plan *p,uint32_t reason,bool verified){p->state.phase=SILPH_KPLAN_RESULT;p->state.reason=reason;p->state.verified=verified;}
static int text(const char *value,size_t bytes){
    if(!value||!value[0])return 0;for(size_t i=0;i<bytes;i++){unsigned char ch=(unsigned char)value[i];if(!ch)return 1;if(ch<=32||ch>=127)return 0;}return 0;
}
static double distance(double a,double b,double c,double d){double r=.017453292519943295,x=sin((a-c)*r/2),y=sin((b-d)*r/2);return 12742000*asin(sqrt(fmax(0,fmin(1,x*x+cos(a*r)*cos(c*r)*y*y))));}
int silph_kecleon_plan_open(void *memory,size_t bytes,const SilphScheduleIdentity *id,uint32_t maximum,uint64_t now){
    if(!memory||bytes!=sizeof(Plan)||!identity_valid(id)||!maximum||maximum>50000||!now||now>UINT64_MAX-UINT64_C(180000000000))return SILPH_MANAGED_ARGUMENT;
    Plan *p=memory;if(p->opened)return SILPH_MANAGED_STATE;memset(p,0,bytes);p->history=calloc(128,sizeof(*p->history));if(!p->history)return SILPH_MANAGED_CAPACITY;
    p->capacity=128;p->opened=1;p->owner=(uint32_t)syscall(__NR_gettid);p->identity=*id;p->maximum=maximum;p->clock=now;p->state.phase=SILPH_KPLAN_NEED_TARGET;p->state.request=1;return 0;
}
int silph_kecleon_plan_target(void *memory,const SilphScheduleIdentity *id,const SilphKecleonCandidate *candidate,uint64_t now){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;
    if(p->state.phase!=SILPH_KPLAN_NEED_TARGET||silph_kecleon_candidate_validate(candidate)||candidate->request!=p->state.request)return SILPH_MANAGED_STATE;
    p->state.candidate=*candidate;p->state.reason=p->state.verified=p->state.move_id=0;
    memset(p->state.fort,0,sizeof(p->state.fort));memset(p->state.incident,0,sizeof(p->state.incident));
    for(unsigned i=0;i<p->used;i++)if(!strcmp(p->history[i],candidate->source_id)){result(p,SILPH_KPLAN_DUPLICATE,true);return 0;}
    p->state.phase=SILPH_KPLAN_NAVIGATE;p->deadline=now+UINT64_C(15000000000);return 0;
}
int silph_kecleon_plan_moved(void *memory,const SilphScheduleIdentity *id,uint64_t request,uint32_t move,bool accepted,uint64_t now){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;
    if(request!=p->state.request||(p->state.phase!=SILPH_KPLAN_NAVIGATE&&p->state.phase!=SILPH_KPLAN_NUDGE)||move!=p->state.move_id)return SILPH_MANAGED_STATE;
    if(!accepted||now>=p->deadline){result(p,SILPH_KPLAN_UNVERIFIED,false);return 0;}
    p->state.phase=SILPH_KPLAN_SCAN;p->state.scan_after_ns=now+UINT64_C(500000000);p->deadline=now+UINT64_C(12000000000);return 0;
}
int silph_kecleon_plan_scan(void *memory,const SilphScheduleIdentity *id,uint64_t request,const SilphGameStop *stop,uint64_t now,uint64_t wall){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;
    if(request!=p->state.request||p->state.phase!=SILPH_KPLAN_SCAN||!wall)return SILPH_MANAGED_STATE;
    if(now<p->state.scan_after_ns)return 0;
    p->state.scan_after_ns=now+UINT64_C(1000000000);
    if(stop&&stop->scan_complete&&stop->matches==1&&text(stop->fort,sizeof(stop->fort))&&isfinite(stop->latitude)&&isfinite(stop->longitude)
        &&fabs(stop->latitude)<=90&&fabs(stop->longitude)<=180&&isfinite(stop->meters)&&stop->meters>=0&&stop->meters<=50&&distance(stop->latitude,stop->longitude,p->state.candidate.latitude,p->state.candidate.longitude)<=3
        &&stop->incident_type==8&&stop->active&&!stop->complete&&text(stop->incident,sizeof(stop->incident))&&stop->expires_ms>wall&&stop->expires_ms<=INT64_MAX){
        memcpy(p->state.fort,stop->fort,sizeof(stop->fort));memcpy(p->state.incident,stop->incident,sizeof(stop->incident));p->state.phase=SILPH_KPLAN_AUTHORIZE;p->deadline=now+UINT64_C(10000000000);
    }else if(now>=p->deadline){
        if(p->state.move_id<2){p->state.move_id++;p->state.phase=SILPH_KPLAN_NUDGE;p->deadline=now+UINT64_C(15000000000);}
        else result(p,SILPH_KPLAN_UNVERIFIED,false);
    }
    return 0;
}
int silph_kecleon_plan_authorize(void *memory,const SilphScheduleIdentity *id,uint64_t request,bool accepted,uint64_t now){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;if(request!=p->state.request||p->state.phase!=SILPH_KPLAN_AUTHORIZE)return SILPH_MANAGED_STATE;
    if(now>=p->deadline)result(p,SILPH_KPLAN_UNVERIFIED,false);
    else if(accepted){p->state.phase=SILPH_KPLAN_RUN;p->deadline=now+UINT64_C(180000000000);}else result(p,SILPH_KPLAN_REJECTED,true);return 0;
}
int silph_kecleon_plan_result(void *memory,const SilphScheduleIdentity *id,uint64_t request,const SilphKecleonMetrics *value,uint64_t now){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;if(!value||request!=p->state.request||p->state.phase!=SILPH_KPLAN_RUN
        ||(value->state!=SILPH_KECLEON_DONE&&value->state!=SILPH_KECLEON_UNVERIFIED)||value->encounters>1||value->catches>value->encounters||value->shinies>value->encounters
        ||value->verified>1||value->uncertain||value->throws>100000||value->berries>value->throws+1||(value->catches&&!value->captured_id))return SILPH_MANAGED_STATE;
    if(p->state.encounters+value->encounters>p->maximum)return SILPH_MANAGED_STATE;
    p->state.encounters+=value->encounters;p->state.catches+=value->catches;p->state.shinies+=value->shinies;p->state.throws+=value->throws;p->state.berries+=value->berries;
    p->state.xp+=value->xp;p->state.stardust+=value->stardust;if(value->reward_error)p->state.reward_error=value->reward_error;
    result(p,value->state==SILPH_KECLEON_UNVERIFIED||value->error?SILPH_KPLAN_UNVERIFIED:value->catches?SILPH_KPLAN_CAPTURED:value->filtered?SILPH_KPLAN_FILTERED:SILPH_KPLAN_UNAVAILABLE,
        value->state==SILPH_KECLEON_DONE&&!value->error&&value->verified);return 0;
}
int silph_kecleon_plan_acknowledge(void *memory,const SilphScheduleIdentity *id,uint64_t request,uint64_t now){
    Plan *p=memory;int status=guard(p,id,now);if(status)return status;if(request!=p->state.request||p->state.phase!=SILPH_KPLAN_RESULT)return SILPH_MANAGED_STATE;
    if(p->state.verified&&p->state.reason!=SILPH_KPLAN_DUPLICATE){
        if(p->used==p->capacity){unsigned next=p->capacity<25000?p->capacity*2:50000;
            if(next<=p->capacity){p->state.phase=SILPH_KPLAN_FINISHED;p->state.reason=SILPH_KPLAN_HISTORY_FULL;return 0;}
            void *memory=realloc(p->history,(size_t)next*sizeof(*p->history));if(!memory)return SILPH_MANAGED_CAPACITY;p->history=memory;p->capacity=next;
        }
        memcpy(p->history[p->used++],p->state.candidate.source_id,sizeof(*p->history));
    }
    if(p->state.encounters>=p->maximum){p->state.phase=SILPH_KPLAN_FINISHED;p->state.reason=SILPH_KPLAN_LIMIT;}
    else if(p->state.request==INT64_MAX)return SILPH_MANAGED_STATE;
    else{p->state.request++;p->state.phase=SILPH_KPLAN_NEED_TARGET;memset(&p->state.candidate,0,sizeof(p->state.candidate));}
    return 0;
}
int silph_kecleon_plan_empty(void *memory,const SilphScheduleIdentity *id,uint64_t request,uint64_t now){Plan *p=memory;int status=guard(p,id,now);if(status)return status;if(request!=p->state.request||p->state.phase!=SILPH_KPLAN_NEED_TARGET)return SILPH_MANAGED_STATE;p->state.phase=SILPH_KPLAN_FINISHED;p->state.reason=SILPH_KPLAN_EMPTY;return 0;}
int silph_kecleon_plan_advance(void *memory,const SilphScheduleIdentity *id,uint64_t now){Plan *p=memory;int status=guard(p,id,now);if(status)return status;
    if((p->state.phase==SILPH_KPLAN_NAVIGATE||p->state.phase==SILPH_KPLAN_NUDGE||p->state.phase==SILPH_KPLAN_AUTHORIZE||p->state.phase==SILPH_KPLAN_RUN)&&now>=p->deadline)result(p,SILPH_KPLAN_UNVERIFIED,false);return 0;}
int silph_kecleon_plan_state(void *memory,SilphKecleonPlanState *out){if(!out)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));Plan *p=memory;if(!p||!p->opened)return SILPH_MANAGED_STATE;if(p->owner!=(uint32_t)syscall(__NR_gettid))return SILPH_MANAGED_THREAD;*out=p->state;return 0;}
int silph_kecleon_plan_close(void *memory){Plan *p=memory;if(!p||!p->opened)return 0;if(p->owner!=(uint32_t)syscall(__NR_gettid))return SILPH_MANAGED_THREAD;free(p->history);memset(p,0,sizeof(*p));return 0;}
