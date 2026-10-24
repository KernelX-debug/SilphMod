#define _POSIX_C_SOURCE 200809L
#include "readback.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SILPH_READBACK_MAGIC UINT64_C(0x53494c5048524231)

typedef struct {
    uint64_t magic;
    SilphItemCount getter;
    const void *method;
    void *klass;
    SilphObjectClass object_class;
    pid_t thread;
    uint64_t batches;
    uint64_t reads;
    uint64_t total_ns;
    uint64_t maximum_ns;
} Readback;

static uint64_t monotonic_ns(void) {
    struct timespec sample;
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0) return 0;
    return (uint64_t)sample.tv_sec * UINT64_C(1000000000) + (uint64_t)sample.tv_nsec;
}

uint32_t silph_readback_abi(void) { return SILPH_READBACK_ABI; }
size_t silph_readback_size(void) { return sizeof(Readback); }

int silph_readback_open(void *memory, size_t bytes, SilphItemCount getter,
                        const void *method, void *klass, SilphObjectClass object_class) {
    if (!memory || bytes != sizeof(Readback) || !getter || !method || !klass || !object_class)
        return SILPH_READBACK_ARGUMENT;
    Readback *state = memory;
    memset(state, 0, sizeof(*state));
    state->getter = getter;
    state->method = method;
    state->klass = klass;
    state->object_class = object_class;
    state->thread = gettid();
    state->magic = SILPH_READBACK_MAGIC;
    return SILPH_READBACK_OK;
}

int silph_readback_counts(void *memory, void *bag, const int32_t *ids,
                          uint32_t count, int32_t *output, uint32_t capacity) {
    if (!memory || !bag || !ids || !output || count == 0 || count > SILPH_READBACK_MAX_ITEMS || capacity < count)
        return SILPH_READBACK_ARGUMENT;
    Readback *state = memory;
    if (state->magic != SILPH_READBACK_MAGIC) return SILPH_READBACK_STATE;
    if (state->thread != gettid()) return SILPH_READBACK_THREAD;
    for (uint32_t index = 0; index < count; index++) {
        output[index] = -1;
        if (ids[index] <= 0 || ids[index] > 1000000) return SILPH_READBACK_ARGUMENT;
        for (uint32_t prior = 0; prior < index; prior++)
            if (ids[prior] == ids[index]) return SILPH_READBACK_ARGUMENT;
    }
    if (state->object_class(bag) != state->klass) return SILPH_READBACK_CLASS;
    const uint64_t began = monotonic_ns();
    int32_t snapshot[SILPH_READBACK_MAX_ITEMS];
    int status = SILPH_READBACK_OK;
    for (uint32_t index = 0; index < count; index++) {
        int32_t value = state->getter(bag, ids[index], state->method);
        state->reads++;
        if (value < 0 || value > 100000) { status = SILPH_READBACK_COUNT; break; }
        snapshot[index] = value;
    }
    const uint64_t ended = monotonic_ns();
    const uint64_t elapsed = began && ended >= began ? ended - began : 0;
    state->batches++;
    state->total_ns += elapsed;
    if (elapsed > state->maximum_ns) state->maximum_ns = elapsed;
    if (status == SILPH_READBACK_OK) memcpy(output, snapshot, count * sizeof(*output));
    memset(snapshot, 0, sizeof(snapshot));
    return status;
}

int silph_readback_metrics(void *memory, uint64_t *output, uint32_t capacity) {
    if (!memory || !output || capacity < 4) return SILPH_READBACK_ARGUMENT;
    Readback *state = memory;
    if (state->magic != SILPH_READBACK_MAGIC) return SILPH_READBACK_STATE;
    if (state->thread != gettid()) return SILPH_READBACK_THREAD;
    output[0] = state->batches;
    output[1] = state->reads;
    output[2] = state->total_ns;
    output[3] = state->maximum_ns;
    return SILPH_READBACK_OK;
}

int silph_readback_close(void *memory) {
    if (!memory) return SILPH_READBACK_ARGUMENT;
    Readback *state = memory;
    if (state->magic != SILPH_READBACK_MAGIC) return SILPH_READBACK_STATE;
    memset(state, 0, sizeof(*state));
    return SILPH_READBACK_OK;
}
