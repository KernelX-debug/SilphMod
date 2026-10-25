#ifndef SILPH_PASS_ACTIONS_H
#define SILPH_PASS_ACTIONS_H
#include "game-read.h"
#include "managed-promise.h"
typedef struct {uint64_t scans,requests,claimed,encounters,xp,stardust,work_ns,max_work_ns;uint32_t state,error,operation,result;} SilphPassMetrics;
size_t silph_pass_size(void);
int silph_pass_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *);
int silph_pass_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,bool,bool,SilphManagedRef *,SilphPassMetrics *);
int silph_pass_close(void *);
int silph_pass_invoke(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,const SilphReadArgument *,uint32_t,SilphReadValue *);
int silph_pass_claim(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef,uint32_t,uint32_t,bool,SilphReadValue *);
int silph_pass_enumerate(void *,SilphManagedRef,uint32_t,SilphReadValue *);
#endif
