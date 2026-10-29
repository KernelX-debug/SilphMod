#ifndef SILPH_BUDDY_CARE_H
#define SILPH_BUDDY_CARE_H
#include "game-read.h"
#include "scheduler.h"
typedef struct {uint32_t flags,count,berries[5];} SilphBuddyCareOptions;
size_t silph_buddy_care_size(void);
int silph_buddy_care_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,const SilphBuddyCareOptions *);
int silph_buddy_care_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,bool,bool *);
int silph_buddy_care_close(void *);
uint32_t silph_buddy_care_floor(void *,const SilphGameSnapshot *,uint32_t);
int silph_buddy_care_invoke(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,const SilphReadArgument *,uint32_t,SilphReadValue *);
int silph_buddy_care_cost(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef,SilphManagedRef,int32_t *);
int silph_buddy_care_mega(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef,uint64_t,uint32_t,SilphReadValue *);
#endif
