#ifndef SILPH_WORK_REFILL_H
#define SILPH_WORK_REFILL_H
#include "scheduler.h"
#include <stdatomic.h>
typedef struct {atomic_uint gate;uint64_t sequence,last;} SilphWorkRefill;
static inline int silph_work_refill(SilphWorkRefill *s,void *scheduler,SilphScheduleLease lease,const SilphScheduleIdentity *identity,uint64_t now,uint64_t interval,uint32_t kind){
    if(!s||!scheduler||!identity||!now||interval<UINT64_C(33000000)||interval>UINT64_C(3000000000)||now>UINT64_MAX-UINT64_C(3000000000)||(kind!=SILPH_JOB_OBSERVE&&kind!=SILPH_JOB_WRITE))return SILPH_SCHEDULE_ARGUMENT;
    unsigned zero=0;if(!atomic_compare_exchange_strong_explicit(&s->gate,&zero,1,memory_order_acquire,memory_order_relaxed))return SILPH_SCHEDULE_BUSY;
    SilphScheduleMetrics metrics;int r=silph_schedule_metrics(scheduler,lease,&metrics);
    if(!r&&metrics.state==SILPH_SCHEDULE_RUNNING&&!metrics.reason&&!metrics.queued&&(!s->last||(now>=s->last&&now-s->last>=interval))){
        if(s->sequence==UINT64_MAX)r=SILPH_SCHEDULE_ORDER;
        else{SilphScheduleJob job={.sequence=s->sequence+1,.deadline_ns=now+UINT64_C(3000000000),.kind=kind};r=silph_schedule_submit(scheduler,lease,&job);if(!r){s->sequence++;s->last=now;}}
    }
    atomic_store_explicit(&s->gate,0,memory_order_release);return r;
}
#endif
