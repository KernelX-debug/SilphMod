#ifndef SILPH_BUDDY_ACTIONS_H
#define SILPH_BUDDY_ACTIONS_H
#include "rpc-actions.h"
enum {SILPH_BUDDY_CHECK=1,SILPH_BUDDY_CHECKING=2,SILPH_BUDDY_REMOVE=3,SILPH_BUDDY_REMOVING=4,SILPH_BUDDY_NONE=5,SILPH_BUDDY_REMOVED=6,SILPH_BUDDY_FAILED=7};
typedef struct {uint32_t state,error,result;uint64_t requests;} SilphBuddyMetrics;
size_t silph_buddy_size(void);
int silph_buddy_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *);
int silph_buddy_tick(void *,const SilphSchedulePermit *,uint64_t,SilphBuddyMetrics *);
int silph_buddy_close(void *);
#endif
