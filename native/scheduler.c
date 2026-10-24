#define _GNU_SOURCE
#include "scheduler.h"
#include <stdatomic.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define MAGIC UINT32_C(0x53534331)
#define MAX_EPOCH (UINT64_MAX >> 8)
#define MAX_JOB_LIFE UINT64_C(30000000000)

typedef struct {
    uint32_t magic;
    atomic_uint gate;
    atomic_uint_fast64_t epoch, stop_ticket, active_epoch, active_sequence;
    atomic_uint active_tid,session_readonly;
    atomic_uint_fast64_t session_until;
    SilphScheduleConfig config;
    SilphScheduleMetrics metrics;
    SilphScheduleJob jobs[SILPH_SCHEDULER_CAPACITY];
    uint64_t pulse_ns, last_sequence, active_deadline;
    uint32_t head, count, foreground, active_kind;
} Schedule;
static atomic_uint_fast64_t epochs = ATOMIC_VAR_INIT(1);
static uint64_t next_epoch(void) {
    uint_fast64_t epoch = atomic_load_explicit(&epochs, memory_order_relaxed);
    while (epoch && epoch <= MAX_EPOCH) {
        if (atomic_compare_exchange_weak_explicit(&epochs, &epoch, epoch + 1, memory_order_relaxed, memory_order_relaxed)) return epoch;
    }
    return 0;
}

static uint32_t tid(void) { return (uint32_t)syscall(__NR_gettid); }
static uint64_t monotonic(void *unused) {
    (void)unused;
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) || value.tv_sec < 0) return 0;
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}
size_t silph_schedule_size(void) { return sizeof(Schedule); }
int silph_schedule_init(void *memory, size_t bytes) {
    if (!memory || bytes != sizeof(Schedule) || (uintptr_t)memory % _Alignof(Schedule)) return SILPH_SCHEDULE_ARGUMENT;
    /* Initialization and storage reclamation require exclusive ownership. */
    memset(memory, 0, bytes);
    Schedule *state = memory;
    atomic_init(&state->gate, 0);atomic_init(&state->epoch, 0);atomic_init(&state->stop_ticket, 0);
    atomic_init(&state->active_epoch, 0);atomic_init(&state->active_sequence, 0);atomic_init(&state->active_tid, 0);
    atomic_init(&state->session_until,0);atomic_init(&state->session_readonly,1);
    state->magic = MAGIC;
    return SILPH_SCHEDULE_OK;
}
static int lock(Schedule *state) {
    if (!state || state->magic != MAGIC) return SILPH_SCHEDULE_ARGUMENT;
    unsigned expected = 0;
    return atomic_compare_exchange_strong_explicit(&state->gate, &expected, 1, memory_order_acquire, memory_order_relaxed)
        ? SILPH_SCHEDULE_OK : SILPH_SCHEDULE_BUSY;
}
static void unlock(Schedule *state) { atomic_store_explicit(&state->gate, 0, memory_order_release); }
static bool identity_valid(const SilphScheduleIdentity *id) {
    return id && id->pid && !id->reserved && id->process_ticks && (id->session_hi || id->session_lo)
        && (id->account_hi || id->account_lo);
}
static bool identity_equal(const SilphScheduleIdentity *a, const SilphScheduleIdentity *b) {
    return identity_valid(b) && a->pid == b->pid && a->process_ticks == b->process_ticks
        && a->session_hi == b->session_hi && a->session_lo == b->session_lo
        && a->account_hi == b->account_hi && a->account_lo == b->account_lo;
}
static bool current(Schedule *state, SilphScheduleLease lease) {
    return lease.epoch && lease.epoch == atomic_load_explicit(&state->epoch, memory_order_acquire);
}
static unsigned reason(Schedule *state, uint64_t epoch) {
    uint64_t ticket = atomic_load_explicit(&state->stop_ticket, memory_order_acquire);
    return ticket >> 8 == epoch ? (unsigned)(ticket & 255) : 0;
}
static void request(Schedule *state, uint64_t epoch, unsigned why) {
    uint_fast64_t ticket = atomic_load_explicit(&state->stop_ticket, memory_order_relaxed);
    while ((ticket >> 8) < epoch) {
        uint_fast64_t next = epoch << 8 | why;
        if (atomic_compare_exchange_weak_explicit(&state->stop_ticket, &ticket, next, memory_order_release, memory_order_relaxed)) break;
    }
}
static bool live(Schedule *state) {
    return state->metrics.state == SILPH_SCHEDULE_RUNNING || state->metrics.state == SILPH_SCHEDULE_PAUSED;
}
static bool timestamp(Schedule *state, uint64_t *now) {
    *now = state->config.clock(state->config.user);
    if (!*now || *now < state->metrics.clock_ns) {
        request(state, state->metrics.epoch, SILPH_STOP_CLOCK);return false;
    }
    state->metrics.clock_ns = *now;return true;
}
static bool heartbeat(Schedule *state, uint64_t now) {
    if (now < state->pulse_ns || now - state->pulse_ns > state->config.heartbeat_timeout_ns) {
        request(state, state->metrics.epoch, SILPH_STOP_HEARTBEAT);return false;
    }
    return true;
}
static void inactive(Schedule *state) {
    atomic_store_explicit(&state->active_epoch, 0, memory_order_release);
    atomic_store_explicit(&state->active_tid, 0, memory_order_release);
    state->active_kind = 0;state->active_deadline = 0;
}
static int finish(Schedule *state) {
    if (!live(state)) return state->metrics.state == SILPH_SCHEDULE_CLOSED ? SILPH_SCHEDULE_OK : SILPH_SCHEDULE_STATE;
    state->metrics.state = SILPH_SCHEDULE_DRAINING;
    atomic_store(&state->session_until,0);
    state->metrics.reason = reason(state, state->metrics.epoch);
    inactive(state);
    state->metrics.canceled += state->count;
    memset(state->jobs, 0, sizeof(state->jobs));state->count = 0;state->head = 0;
    state->metrics.cleanup_calls++;
    int cleanup = state->config.cleanup(state->config.user);
    state->metrics.state = cleanup ? SILPH_SCHEDULE_UNCERTAIN : SILPH_SCHEDULE_CLOSED;
    return cleanup ? SILPH_SCHEDULE_CLEANUP : SILPH_SCHEDULE_OK;
}
int silph_schedule_open(void *memory, const SilphScheduleConfig *config, SilphScheduleLease *output) {
    if (!output) return SILPH_SCHEDULE_ARGUMENT;
    output->epoch = 0;
    if (!config || !identity_valid(&config->identity) || !config->owner_tid || !config->execute || !config->cleanup
        || config->readonly > 1 || config->reserved || !config->max_jobs_per_tick || config->max_jobs_per_tick > 4
        || config->heartbeat_timeout_ns < UINT64_C(100000000) || config->heartbeat_timeout_ns > UINT64_C(10000000000)
        || config->tick_budget_ns < UINT64_C(100000) || config->tick_budget_ns > UINT64_C(10000000)) return SILPH_SCHEDULE_ARGUMENT;
    Schedule *state = memory;int result = lock(state);if (result) return result;
    if (state->metrics.state != SILPH_SCHEDULE_IDLE && state->metrics.state != SILPH_SCHEDULE_CLOSED) result = SILPH_SCHEDULE_STATE;
    else {
        uint64_t epoch = next_epoch();
        if (!epoch) result = SILPH_SCHEDULE_STATE;
        else {
            SilphScheduleConfig next = *config;
            if (!next.clock) next.clock = monotonic;
            uint64_t now = next.clock(next.user);
            if (!now) result = SILPH_SCHEDULE_ARGUMENT;
            else {
                state->config = next;state->pulse_ns = now;state->last_sequence = 0;
                state->head = 0;state->count = 0;state->foreground = 0;
                memset(state->jobs, 0, sizeof(state->jobs));memset(&state->metrics, 0, sizeof(state->metrics));
                inactive(state);state->metrics.epoch = epoch;
                state->metrics.clock_ns = now;state->metrics.readonly = config->readonly;
                state->metrics.owner_tid = config->owner_tid;state->metrics.state = SILPH_SCHEDULE_PAUSED;
                atomic_store(&state->session_readonly,config->readonly);atomic_store(&state->session_until,monotonic(NULL)+config->heartbeat_timeout_ns);
                atomic_store_explicit(&state->epoch, epoch, memory_order_release);output->epoch = epoch;
            }
        }
    }
    unlock(state);return result;
}
int silph_schedule_stop(void *memory, SilphScheduleLease lease) {
    Schedule *state = memory;
    if (!state || state->magic != MAGIC) return SILPH_SCHEDULE_ARGUMENT;
    if (!current(state, lease)) return SILPH_SCHEDULE_STALE;
    request(state, lease.epoch, SILPH_STOP_USER);
    return SILPH_SCHEDULE_OK;
}
int silph_schedule_pulse(void *memory, SilphScheduleLease lease, const SilphScheduleIdentity *identity, bool foreground) {
    Schedule *state = memory;int result = lock(state);if (result) return result;
    if (!current(state, lease)) result = SILPH_SCHEDULE_STALE;
    else if (!live(state) || reason(state, lease.epoch)) result = SILPH_SCHEDULE_STATE;
    else if (!identity_equal(&state->config.identity, identity)) { request(state, lease.epoch, SILPH_STOP_IDENTITY);result = SILPH_SCHEDULE_STALE; }
    else {
        uint64_t now;
        if (!timestamp(state, &now) || !heartbeat(state, now)) result = SILPH_SCHEDULE_STATE;
        else { state->pulse_ns = now;state->foreground = foreground;state->metrics.state = foreground ? SILPH_SCHEDULE_RUNNING : SILPH_SCHEDULE_PAUSED;atomic_store(&state->session_until,monotonic(NULL)+state->config.heartbeat_timeout_ns); }
    }
    unlock(state);return result;
}
int silph_schedule_submit(void *memory, SilphScheduleLease lease, const SilphScheduleJob *job) {
    if (!job || !job->sequence || job->reserved || (job->kind != SILPH_JOB_OBSERVE && job->kind != SILPH_JOB_WRITE)) return SILPH_SCHEDULE_ARGUMENT;
    Schedule *state = memory;int result = lock(state);if (result) return result;
    if (!current(state, lease)) result = SILPH_SCHEDULE_STALE;
    else if (!live(state) || reason(state, lease.epoch)) result = SILPH_SCHEDULE_STATE;
    else if (state->config.readonly && job->kind == SILPH_JOB_WRITE) result = SILPH_SCHEDULE_DENIED;
    else if (job->sequence <= state->last_sequence) result = SILPH_SCHEDULE_ORDER;
    else {
        uint64_t now;
        if (!timestamp(state, &now) || !heartbeat(state, now)) result = SILPH_SCHEDULE_STATE;
        else if (job->deadline_ns <= now || job->deadline_ns - now > MAX_JOB_LIFE) result = SILPH_SCHEDULE_ARGUMENT;
        else if (state->count == SILPH_SCHEDULER_CAPACITY) result = SILPH_SCHEDULE_CAPACITY_ERROR;
        else {
            state->jobs[(state->head + state->count) % SILPH_SCHEDULER_CAPACITY] = *job;
            state->last_sequence = job->sequence;state->count++;state->metrics.submitted++;
        }
    }
    unlock(state);return result;
}
bool silph_schedule_permitted(const SilphSchedulePermit *permit, bool write) {
    if (!permit || !permit->context || !permit->epoch || !permit->sequence) return false;
    Schedule *state = permit->context;
    if (state->magic != MAGIC || atomic_load_explicit(&state->active_tid, memory_order_acquire) != tid()
        || atomic_load_explicit(&state->active_epoch, memory_order_acquire) != permit->epoch
        || atomic_load_explicit(&state->active_sequence, memory_order_acquire) != permit->sequence) return false;
    uint64_t now;
    return !reason(state, permit->epoch) && (!write || (!state->config.readonly && state->active_kind == SILPH_JOB_WRITE))
        && state->foreground && timestamp(state, &now) && heartbeat(state, now) && now < state->active_deadline;
}
bool silph_schedule_visual_allowed(void *memory,SilphScheduleLease lease) {
    Schedule *state=memory;if(lock(state))return false;
    uint64_t now=0;
    bool allowed=current(state,lease)&&tid()==state->config.owner_tid&&live(state)&&!state->config.readonly
        &&!reason(state,lease.epoch)&&state->foreground&&timestamp(state,&now)&&heartbeat(state,now);
    unlock(state);return allowed;
}
bool silph_schedule_session_alive(void *memory,SilphScheduleLease lease){
    Schedule *state=memory;if(!state||state->magic!=MAGIC||!current(state,lease)||atomic_load(&state->session_readonly)||reason(state,lease.epoch))return false;
    uint64_t until=atomic_load(&state->session_until),now=monotonic(NULL);return now&&until&&now<=until&&current(state,lease)&&!reason(state,lease.epoch);
}
int silph_schedule_tick(void *memory, SilphScheduleLease lease, const SilphScheduleIdentity *identity, bool visible) {
    Schedule *state = memory;int result = lock(state);if (result) return result;
    if (!current(state, lease)) result = SILPH_SCHEDULE_STALE;
    else if (tid() != state->config.owner_tid) result = SILPH_SCHEDULE_THREAD;
    else if (!live(state)) result = SILPH_SCHEDULE_STATE;
    else {
        state->metrics.ticks++;
        if (!identity_equal(&state->config.identity, identity)) request(state, lease.epoch, SILPH_STOP_IDENTITY);
        uint64_t start = 0;
        timestamp(state, &start);heartbeat(state, start);
        if (reason(state, lease.epoch)) result = finish(state);
        else if (!visible || !state->foreground) state->metrics.skipped++;
        else {
            for (uint32_t work = 0; state->count && work < state->config.max_jobs_per_tick; work++) {
                uint64_t now;
                if (!timestamp(state, &now) || !heartbeat(state, now) || reason(state, lease.epoch)) break;
                if (now - start >= state->config.tick_budget_ns) { state->metrics.budget_exhausted++;break; }
                SilphScheduleJob job = state->jobs[state->head];
                memset(&state->jobs[state->head], 0, sizeof(job));state->head = (state->head + 1) % SILPH_SCHEDULER_CAPACITY;state->count--;
                if (now >= job.deadline_ns) { state->metrics.expired++;continue; }
                state->active_kind = job.kind;state->active_deadline = job.deadline_ns;
                atomic_store_explicit(&state->active_sequence, job.sequence, memory_order_release);
                atomic_store_explicit(&state->active_tid, tid(), memory_order_release);
                atomic_store_explicit(&state->active_epoch, lease.epoch, memory_order_release);
                SilphSchedulePermit permit = {state, lease.epoch, job.sequence};
                int execution = 0;
                if (silph_schedule_permitted(&permit, job.kind == SILPH_JOB_WRITE)) {
                    execution = state->config.execute(state->config.user, &job, &permit);state->metrics.executed++;
                } else state->metrics.canceled++;
                inactive(state);
                uint64_t after;
                if (timestamp(state, &after) && after >= now) {
                    heartbeat(state, after);
                    uint64_t elapsed = after - now;
                    state->metrics.callback_ns += elapsed;
                    if (elapsed > state->metrics.max_callback_ns) state->metrics.max_callback_ns = elapsed;
                }
                if (execution) request(state, lease.epoch, SILPH_STOP_CALLBACK);
            }
            if (reason(state, lease.epoch)) result = finish(state);
        }
    }
    unlock(state);return result;
}
int silph_schedule_metrics(void *memory, SilphScheduleLease lease, SilphScheduleMetrics *output) {
    if (!output) return SILPH_SCHEDULE_ARGUMENT;
    memset(output, 0, sizeof(*output));
    Schedule *state = memory;int result = lock(state);if (result) return result;
    if (!current(state, lease)) result = SILPH_SCHEDULE_STALE;
    else {
        *output = state->metrics;output->queued = state->count;output->reason = reason(state, lease.epoch);
        if (live(state) && output->reason) output->state = SILPH_SCHEDULE_STOP_REQUESTED;
    }
    unlock(state);return result;
}
