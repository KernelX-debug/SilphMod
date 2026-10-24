#ifndef SILPH_SESSION_LINK_H
#define SILPH_SESSION_LINK_H
#include "session-wire.h"
#include "scheduler.h"
#include "kecleon-wire.h"
size_t silph_session_link_size(void);
int silph_session_link_open(void *,size_t,const SilphScheduleIdentity *,uint32_t);
int silph_session_link_message(void *,const void *,size_t);
int silph_session_link_readonly_message(void *,const void *,size_t);
int silph_session_link_config(void *,SilphSessionConfig *);
int silph_session_link_publish(void *,const SilphSessionSnapshot *);
int silph_session_link_snapshot(void *,void *,size_t,size_t *);
int silph_session_link_close(void *);
int silph_session_link_pop(void *,SilphKecleonCommand *);
int silph_session_link_event(void *,const SilphKecleonEvent *);
#endif
