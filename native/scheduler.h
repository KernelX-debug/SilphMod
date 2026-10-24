#ifndef SILPH_SCHEDULER_H
#define SILPH_SCHEDULER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SILPH_SCHEDULER_ABI 1u
#define SILPH_SCHEDULER_CAPACITY 32u
#define SILPH_SCHEDULER_EXPORT __attribute__((visibility("default")))

typedef struct {
    uint64_t process_ticks, session_hi, session_lo, account_hi, account_lo;
    uint32_t pid, reserved;
} SilphScheduleIdentity;
typedef struct { uint64_t epoch; } SilphScheduleLease;
typedef struct {
    uint64_t sequence, deadline_ns, argument;
    uint32_t kind, reserved;
} SilphScheduleJob;
typedef struct { void *context;uint64_t epoch, sequence; } SilphSchedulePermit;
typedef struct {
    uint64_t epoch, submitted, executed, expired, canceled, ticks, skipped, budget_exhausted;
    uint64_t callback_ns, max_callback_ns, clock_ns;
    uint32_t state, reason, queued, owner_tid, cleanup_calls, readonly;
} SilphScheduleMetrics;
typedef uint64_t (*SilphScheduleClock)(void *);
typedef int (*SilphScheduleExecute)(void *, const SilphScheduleJob *, const SilphSchedulePermit *);
typedef int (*SilphScheduleCleanup)(void *);
typedef struct {
    SilphScheduleIdentity identity;
    uint64_t heartbeat_timeout_ns, tick_budget_ns;
    uint32_t owner_tid, max_jobs_per_tick, readonly, reserved;
    SilphScheduleClock clock;
    SilphScheduleExecute execute;
    SilphScheduleCleanup cleanup;
    void *user;
} SilphScheduleConfig;

enum { SILPH_JOB_OBSERVE = 1, SILPH_JOB_WRITE = 2 };
enum {
    SILPH_SCHEDULE_IDLE = 0, SILPH_SCHEDULE_RUNNING = 1, SILPH_SCHEDULE_PAUSED = 2,
    SILPH_SCHEDULE_DRAINING = 3, SILPH_SCHEDULE_CLOSED = 4, SILPH_SCHEDULE_UNCERTAIN = 5,
    SILPH_SCHEDULE_STOP_REQUESTED = 6
};
enum {
    SILPH_STOP_NONE = 0, SILPH_STOP_USER = 1, SILPH_STOP_HEARTBEAT = 2,
    SILPH_STOP_IDENTITY = 3, SILPH_STOP_CLOCK = 4, SILPH_STOP_CALLBACK = 5
};
enum {
    SILPH_SCHEDULE_OK = 0, SILPH_SCHEDULE_ARGUMENT = 1, SILPH_SCHEDULE_BUSY = 2,
    SILPH_SCHEDULE_STATE = 3, SILPH_SCHEDULE_STALE = 4, SILPH_SCHEDULE_THREAD = 5,
    SILPH_SCHEDULE_CAPACITY_ERROR = 6, SILPH_SCHEDULE_ORDER = 7,
    SILPH_SCHEDULE_DENIED = 8, SILPH_SCHEDULE_CLEANUP = 9
};

SILPH_SCHEDULER_EXPORT size_t silph_schedule_size(void);
SILPH_SCHEDULER_EXPORT int silph_schedule_init(void *, size_t);
SILPH_SCHEDULER_EXPORT int silph_schedule_open(void *, const SilphScheduleConfig *, SilphScheduleLease *);
SILPH_SCHEDULER_EXPORT int silph_schedule_pulse(void *, SilphScheduleLease, const SilphScheduleIdentity *, bool);
SILPH_SCHEDULER_EXPORT int silph_schedule_submit(void *, SilphScheduleLease, const SilphScheduleJob *);
SILPH_SCHEDULER_EXPORT int silph_schedule_stop(void *, SilphScheduleLease);
SILPH_SCHEDULER_EXPORT int silph_schedule_tick(void *, SilphScheduleLease, const SilphScheduleIdentity *, bool);
SILPH_SCHEDULER_EXPORT bool silph_schedule_permitted(const SilphSchedulePermit *, bool);
SILPH_SCHEDULER_EXPORT bool silph_schedule_visual_allowed(void *, SilphScheduleLease);
SILPH_SCHEDULER_EXPORT bool silph_schedule_session_alive(void *, SilphScheduleLease);
SILPH_SCHEDULER_EXPORT int silph_schedule_metrics(void *, SilphScheduleLease, SilphScheduleMetrics *);
#endif
