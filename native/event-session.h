#ifndef SILPH_EVENT_SESSION_H
#define SILPH_EVENT_SESSION_H
#include "game-read.h"
#include "managed-promise.h"
typedef struct {
    uint64_t requested,opened,failed,candies,rare,xl,rare_xl,xp,stardust,encounters,examined,scans,work_ns,max_work_ns;
    uint32_t pending,error,reward_error;
} SilphEventMetrics;
size_t silph_events_size(void);
int silph_events_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *);
int silph_events_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,uint64_t,SilphEventMetrics *);
int silph_events_close(void *);
int silph_game_read_event_targets(void *,SilphManagedRef *,SilphManagedRef *);
int silph_event_process(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef,SilphReadValue *);
#endif
