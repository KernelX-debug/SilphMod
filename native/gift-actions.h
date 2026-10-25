#ifndef SILPH_GIFT_ACTIONS_H
#define SILPH_GIFT_ACTIONS_H
#include "game-read.h"
#include "scheduler.h"
typedef struct {uint32_t state,error,operation,result;uint64_t opened,sent,requests,skipped,xp,stardust,converted;} SilphGiftMetrics;
size_t silph_gift_size(void);
int silph_gift_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,uint32_t);
int silph_gift_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,SilphGiftMetrics *);
int silph_gift_close(void *);
int silph_gift_invoke(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,const SilphReadArgument *,uint32_t,SilphReadValue *);
int silph_gift_open_stardust(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef,uint64_t,SilphReadValue *);
int silph_gift_enumerate(void *,SilphManagedRef,uint32_t,SilphReadValue *);
#endif
