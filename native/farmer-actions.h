#ifndef SILPH_FARMER_ACTIONS_H
#define SILPH_FARMER_ACTIONS_H
#include "game-read.h"
#include "managed-action.h"
#include "managed-promise.h"
enum { SILPH_FARMER_SELECT=1,SILPH_FARMER_ENCOUNTER=2,SILPH_FARMER_CAPTURE=3,SILPH_FARMER_DONE=4,SILPH_FARMER_RETRY=5,
    SILPH_FARMER_OWNED=6,SILPH_FARMER_TRANSFER=7,SILPH_FARMER_SYNC=8,SILPH_FARMER_BERRY=9 };
typedef struct { uint64_t encounters,throws,catches,captured_id,target_id,fresh_response,transfer_requests,transferred;
    uint32_t state,status,species,error,transfer_decision,transfer_reasons,transfer_error,scope_granted;
    uint64_t berries,fled,skipped,berry_requests;
    uint64_t xp,stardust;uint32_t reward_error;
    uint64_t research_catches,research_transferred;
    uint32_t promise_phase,promise_state,promise_reason;
} SilphFarmerMetrics;
typedef struct {
    uint32_t abi,continuous,max_catches,quality,curve,use_berries,keep_options,berry_count;
    int32_t berries[5];
    uint8_t only_species[256];
} SilphFarmerOptions;
size_t silph_farmer_actions_size(void);
int silph_farmer_actions_metrics(void *,SilphFarmerMetrics *);
int silph_farmer_actions_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,uint32_t);
int silph_farmer_actions_configure(void *,const SilphFarmerOptions *);
int silph_farmer_research_target(void *,SilphManagedRef,uint64_t,uint32_t);
bool silph_farmer_actions_inventory_due(void *);
int silph_farmer_actions_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,SilphFarmerMetrics *);
int silph_farmer_actions_close(void *);
#endif
