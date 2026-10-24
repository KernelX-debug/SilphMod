#ifndef SILPH_KECLEON_PLAN_H
#define SILPH_KECLEON_PLAN_H
#include "kecleon-actions.h"
enum {SILPH_KPLAN_NEED_TARGET=1,SILPH_KPLAN_NAVIGATE=2,SILPH_KPLAN_SCAN=3,SILPH_KPLAN_AUTHORIZE=4,SILPH_KPLAN_RUN=5,
    SILPH_KPLAN_NUDGE=6,SILPH_KPLAN_RESULT=7,SILPH_KPLAN_FINISHED=8};
enum {SILPH_KPLAN_CAPTURED=1,SILPH_KPLAN_FILTERED=2,SILPH_KPLAN_UNAVAILABLE=3,SILPH_KPLAN_UNVERIFIED=4,SILPH_KPLAN_LIMIT=5,
    SILPH_KPLAN_EMPTY=6,SILPH_KPLAN_DUPLICATE=7,SILPH_KPLAN_REJECTED=8,SILPH_KPLAN_ACCOUNT=9,SILPH_KPLAN_HISTORY_FULL=10};
typedef struct {
    SilphKecleonCandidate candidate;
    uint64_t request,encounters,catches,shinies,throws,berries,scan_after_ns;
    uint32_t phase,reason,verified,move_id;
    char fort[161],incident[161];
    uint64_t xp,stardust;uint32_t reward_error;
} SilphKecleonPlanState;
size_t silph_kecleon_plan_size(void);
int silph_kecleon_plan_open(void *,size_t,const SilphScheduleIdentity *,uint32_t,uint64_t);
int silph_kecleon_plan_target(void *,const SilphScheduleIdentity *,const SilphKecleonCandidate *,uint64_t);
int silph_kecleon_plan_moved(void *,const SilphScheduleIdentity *,uint64_t,uint32_t,bool,uint64_t);
int silph_kecleon_plan_scan(void *,const SilphScheduleIdentity *,uint64_t,const SilphGameStop *,uint64_t,uint64_t);
int silph_kecleon_plan_advance(void *,const SilphScheduleIdentity *,uint64_t);
int silph_kecleon_plan_authorize(void *,const SilphScheduleIdentity *,uint64_t,bool,uint64_t);
int silph_kecleon_plan_result(void *,const SilphScheduleIdentity *,uint64_t,const SilphKecleonMetrics *,uint64_t);
int silph_kecleon_plan_acknowledge(void *,const SilphScheduleIdentity *,uint64_t,uint64_t);
int silph_kecleon_plan_empty(void *,const SilphScheduleIdentity *,uint64_t,uint64_t);
int silph_kecleon_plan_state(void *,SilphKecleonPlanState *);
int silph_kecleon_plan_close(void *);
#endif
