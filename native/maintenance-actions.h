#ifndef SILPH_MAINTENANCE_ACTIONS_H
#define SILPH_MAINTENANCE_ACTIONS_H
#include "game-read.h"
#include "rpc-actions.h"
typedef struct {
    uint32_t state,error,result,requests,spins,discarded;
    uint64_t cooldown_ms,xp;
    char fort[161];
    uint32_t item,floor,before,after,requested;
    uint64_t stardust;uint32_t reward_error;
} SilphMaintenanceMetrics;
size_t silph_maintenance_size(void);
int silph_maintenance_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *);
int silph_maintenance_spin(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t);
int silph_maintenance_spin_filtered(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,SilphStopPredicate,void *);
int silph_maintenance_discard(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,int32_t,uint32_t,uint32_t,uint64_t);
uint32_t silph_discard_count(uint32_t,uint32_t,uint32_t);
int silph_maintenance_poll(void *,const SilphScheduleIdentity *,uint64_t,SilphMaintenanceMetrics *);
int silph_maintenance_close(void *);
#endif
