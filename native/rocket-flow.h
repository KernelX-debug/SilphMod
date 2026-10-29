#ifndef SILPH_ROCKET_FLOW_H
#define SILPH_ROCKET_FLOW_H
#include "visual-tools.h"
#include "scheduler.h"
size_t silph_rocket_flow_size(void);
int silph_rocket_flow_open(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,SilphVisualAllowed,void *,SilphVisualAddress);
int silph_rocket_flow_bind(void *,SilphManagedRef,const char *,uint64_t,uint32_t);
void silph_rocket_flow_permit(const SilphSchedulePermit *);
void silph_rocket_flow_unbind(void);
bool silph_rocket_flow_busy(void);
bool silph_rocket_flow_challenged(void *,SilphManagedRef);
int silph_rocket_flow_close(void *);
#endif
