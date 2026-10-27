#ifndef SILPH_EVENT_ACTIONS_H
#define SILPH_EVENT_ACTIONS_H
#include "game-read.h"
#include "scheduler.h"
#include "visual-tools.h"
size_t silph_event_size(void);
int silph_event_open(void *,size_t,void *,void *,void *);
int silph_event_tick(void *,const SilphSchedulePermit *,uint64_t,uint64_t,bool);
int silph_event_close(void *);
int silph_game_read_event_service(void *,SilphManagedRef *);
int silph_event_tap(void *,const SilphSchedulePermit *,SilphManagedRef,SilphManagedRef);
int silph_event_view_open(const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,void *,SilphVisualAllowed,SilphVisualAddress);
int silph_event_view_close(void);
bool silph_event_owned(uint64_t);
int silph_event_expect(uint64_t);
void silph_event_complete(uint64_t);
void silph_event_captured(uint64_t);
#endif
