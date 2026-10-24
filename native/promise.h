#ifndef SILPH_PROMISE_H
#define SILPH_PROMISE_H
#include "scheduler.h"

#define SILPH_PROMISE_ABI 1u
#define SILPH_PROMISE_EXPORT __attribute__((visibility("default")))
typedef struct { uint64_t epoch, action, encounter; } SilphPromiseTicket;
typedef struct {
    uint64_t epoch, action, encounter, started_ns, deadline_ns, last_ns, polls;
    uint32_t state, reason, observed_pending, owner_tid;
} SilphPromiseMetrics;
enum {
    SILPH_PROMISE_IDLE=0, SILPH_PROMISE_WAITING=1, SILPH_PROMISE_RESOLVED=2, SILPH_PROMISE_FAILED=3,
    SILPH_PROMISE_TIMED_OUT=4, SILPH_PROMISE_CANCELED=5, SILPH_PROMISE_UNCERTAIN=6, SILPH_PROMISE_CONSUMED=7
};
enum {
    SILPH_PROMISE_REASON_NONE=0, SILPH_PROMISE_REASON_FLAGS=1, SILPH_PROMISE_REASON_TIMEOUT=2,
    SILPH_PROMISE_REASON_CLOCK=3, SILPH_PROMISE_REASON_IDENTITY=4, SILPH_PROMISE_REASON_CANCEL=5
};
enum { SILPH_PROMISE_OK=0, SILPH_PROMISE_ARGUMENT=1, SILPH_PROMISE_STATE=2, SILPH_PROMISE_STALE=3, SILPH_PROMISE_THREAD=4 };

SILPH_PROMISE_EXPORT size_t silph_promise_size(void);
SILPH_PROMISE_EXPORT int silph_promise_init(void *, size_t);
SILPH_PROMISE_EXPORT int silph_promise_begin(void *, const SilphScheduleIdentity *, uint64_t, uint64_t, uint64_t, uint64_t, SilphPromiseTicket *);
SILPH_PROMISE_EXPORT int silph_promise_observe(void *, SilphPromiseTicket, const SilphScheduleIdentity *, uint64_t, uint32_t, uint32_t, uint32_t);
SILPH_PROMISE_EXPORT int silph_promise_check(void *, SilphPromiseTicket, const SilphScheduleIdentity *, uint64_t);
SILPH_PROMISE_EXPORT int silph_promise_consume(void *, SilphPromiseTicket, const SilphScheduleIdentity *, uint64_t, bool *);
SILPH_PROMISE_EXPORT int silph_promise_cancel(void *, SilphPromiseTicket);
SILPH_PROMISE_EXPORT int silph_promise_metrics(void *, SilphPromiseTicket, SilphPromiseMetrics *);
#endif
