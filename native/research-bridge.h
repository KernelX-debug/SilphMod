#ifndef SILPH_RESEARCH_BRIDGE_H
#define SILPH_RESEARCH_BRIDGE_H
#include "managed-action.h"
enum {SILPH_RESEARCH_LIST=1,SILPH_RESEARCH_BUSY=2,SILPH_RESEARCH_RECEIVE=3,SILPH_RESEARCH_REDEEM=4,
    SILPH_RESEARCH_CLAIM=5,SILPH_RESEARCH_REMOVE=6,SILPH_RESEARCH_REWARD=7,SILPH_RESEARCH_BEGIN=8,SILPH_RESEARCH_CREATE=9,
    SILPH_RESEARCH_STAMP_FULL=10,SILPH_RESEARCH_STAMP_DATA=11,SILPH_RESEARCH_STAMP_CLAIM=12,SILPH_RESEARCH_WEEKLY_REWARD=13};
int silph_research_factory_type(void *,SilphManagedRef *);
int silph_research_enable(void *,SilphManagedResolver,void *);
int silph_research_invoke(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,const SilphReadArgument *,uint32_t,SilphReadValue *);
int silph_research_destroy(void *,SilphManagedRef);
#endif
