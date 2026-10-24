#ifndef SILPH_MANAGED_PROMISE_H
#define SILPH_MANAGED_PROMISE_H
#include "managed-read.h"
#include "promise.h"
size_t silph_managed_promise_size(void);
int silph_managed_promise_open(void *,size_t,void *,void *,SilphManagedRef,const SilphScheduleIdentity *,
    uint64_t,uint64_t,uint64_t,const char *,const char *);
int silph_managed_promise_open_rpc(void *,size_t,void *,void *,SilphManagedRef,const SilphScheduleIdentity *,
    uint64_t,uint64_t,uint64_t,const char *,const char *);
int silph_managed_promise_poll(void *,const SilphScheduleIdentity *,uint64_t,SilphPromiseMetrics *);
int silph_managed_promise_take(void *,const SilphScheduleIdentity *,uint64_t,SilphManagedRef *,bool *);
int silph_managed_promise_close(void *);
#endif
