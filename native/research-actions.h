#ifndef SILPH_RESEARCH_ACTIONS_H
#define SILPH_RESEARCH_ACTIONS_H
#include "game-read.h"
#include "research-bridge.h"
typedef struct {uint64_t scans,claimed,discarded,encounters,xp,stardust,requests,weekly_claimed;uint32_t state,error,operation,result,reward_error,weekly_error;} SilphResearchMetrics;
size_t silph_research_size(void);
int silph_research_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,uint32_t,uint32_t);
bool silph_research_due(void *,uint64_t);
bool silph_research_pending(void *);
void silph_research_refresh(void *,uint64_t);
bool silph_research_available(void *);
int silph_research_supply(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,SilphManagedRef,uint64_t);
uint32_t silph_research_target_source(void *,uint64_t);
int silph_research_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,bool,SilphManagedRef *,uint64_t *,uint32_t *);
int silph_research_metrics(void *,SilphResearchMetrics *);
int silph_research_close(void *);
#endif
