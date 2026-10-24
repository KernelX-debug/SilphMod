#ifndef SILPH_KECLEON_SESSION_H
#define SILPH_KECLEON_SESSION_H
#include "session-link.h"
#include "maintenance-actions.h"
typedef struct {SilphKecleonPlanState plan;SilphKecleonMetrics current;uint64_t deleted;uint32_t error;} SilphKecleonSessionMetrics;
size_t silph_kecleon_session_size(void);
int silph_kecleon_session_open(void *,size_t,void *,void *,void *,void *,const SilphScheduleIdentity *,const SilphSessionConfig *,uint64_t);
bool silph_kecleon_session_inventory_due(void *);
int silph_kecleon_session_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,uint64_t,SilphKecleonSessionMetrics *);
int silph_kecleon_session_publish(void *);
int silph_kecleon_session_close(void *);
#endif
