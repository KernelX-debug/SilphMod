#ifndef SILPH_KECLEON_ACTIONS_H
#define SILPH_KECLEON_ACTIONS_H
#include "game-read.h"
#include "rpc-actions.h"
enum { SILPH_KECLEON_WAIT=1,SILPH_KECLEON_GET=2,SILPH_KECLEON_OPEN=3,SILPH_KECLEON_BERRY=4,SILPH_KECLEON_CAPTURE=5,
    SILPH_KECLEON_DONE=6,SILPH_KECLEON_UNVERIFIED=7 };
typedef struct {
    uint64_t encounters,throws,catches,shinies,berries,encounter,captured_id;
    uint32_t state,status,error,verified,cp,attack,defense,stamina,shiny,filtered,requests,fresh_capture;
    uint64_t xp,stardust;uint32_t reward_error,uncertain;
} SilphKecleonMetrics;
typedef struct {uint32_t abi,filters,quality,curve,use_berries,berry_count,max_throws;int32_t berries[5];} SilphKecleonOptions;
typedef struct {uint64_t request;double latitude,longitude;char source_id[97];} SilphKecleonCandidate;
size_t silph_kecleon_actions_size(void);
int silph_kecleon_actions_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,const SilphKecleonOptions *);
int silph_kecleon_bind_candidate(void *,const SilphKecleonCandidate *);
int silph_kecleon_bind_stop(void *,const char *,const char *);
int silph_kecleon_candidate_validate(const SilphKecleonCandidate *);
int silph_kecleon_actions_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,uint64_t,SilphKecleonMetrics *);
int silph_kecleon_actions_close(void *);
bool silph_kecleon_inventory_due(void *);
int silph_kecleon_accept(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
#endif
