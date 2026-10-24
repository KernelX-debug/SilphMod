#include "core.h"
#if !defined(__wasm__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define CORE_MAGIC UINT32_C(0x53434f52)
enum { CAPTURED = 1, SENT = 2, RELEASED = 3 };
typedef struct {
    uint64_t id, encounter;
    uint32_t state, source;
} Entry;
typedef struct {
    uint32_t magic, bytes, capacity, slots, count, confirmed, limit, owner;
    uint32_t entered, writable, read_only, pending_source;
    uint64_t now, beat, sequence, pending_sequence, pending_encounter, pending_sent;
    uint64_t ticks, writes;
    Entry entries[];
} Core;

static uint32_t thread_id(void) {
#if defined(__wasm__)
    return 1;
#else
    return (uint32_t)syscall(__NR_gettid);
#endif
}
static void clear_bytes(void *address, uint32_t bytes) {
    volatile unsigned char *target = address;
    for (uint32_t index = 0; index < bytes; index++) target[index] = 0;
}
static uint32_t slot_count(uint32_t capacity) {
    if (!capacity || capacity > SILPH_CORE_MAX_CAPTURES) return 0;
    uint32_t slots = 2;
    while (slots < capacity * 2) slots <<= 1;
    return slots;
}
uint32_t silph_core_abi(void) { return SILPH_CORE_ABI; }
uint32_t silph_core_size(uint32_t capacity) {
    uint32_t slots = slot_count(capacity);
    return slots ? (uint32_t)sizeof(Core) + slots * (uint32_t)sizeof(Entry) : 0;
}
static Core *valid(void *state) {
    Core *core = state;
    if (!core || core->magic != CORE_MAGIC || core->bytes != silph_core_size(core->capacity)
        || core->slots != slot_count(core->capacity) || core->count > core->capacity) return NULL;
    return core;
}
static Entry *find(Core *core, uint64_t id) {
    if (!id) return NULL;
    uint64_t hash = id;
    hash ^= hash >> 33;hash *= UINT64_C(0xff51afd7ed558ccd);hash ^= hash >> 33;
    uint32_t index = (uint32_t)hash & (core->slots - 1);
    for (uint32_t attempts = 0; attempts < core->slots; attempts++, index = (index + 1) & (core->slots - 1)) {
        Entry *entry = &core->entries[index];
        if (!entry->id || entry->id == id) return entry;
    }
    return NULL;
}
int silph_core_open(void *state, uint32_t bytes, uint32_t capacity, uint32_t limit) {
    uint32_t expected = silph_core_size(capacity);
    if (!state || !expected || bytes != expected || limit > 1000) return -1;
    clear_bytes(state, bytes);
    Core *core = state;
    core->magic = CORE_MAGIC;core->bytes = bytes;core->capacity = capacity;
    core->slots = slot_count(capacity);core->limit = limit;
    return 0;
}
int silph_core_enter(void *state, uint64_t now, uint64_t beat, uint32_t allowed, uint32_t read_only) {
    Core *core = valid(state);
    if (!core || allowed > 1 || read_only > 1) return -1;
    uint32_t tid = thread_id();
    if (core->entered || (core->owner && core->owner != tid)) return -2;
    core->owner = tid;core->entered = 1;core->now = now;core->beat = beat;
    core->writable = allowed;core->read_only = read_only;core->ticks++;
    return 0;
}
int silph_core_leave(void *state) {
    Core *core = valid(state);
    if (!core || !core->entered || core->owner != thread_id()) return -1;
    core->entered = 0;core->writable = 0;
    return 0;
}
int silph_core_write(void *state, uint64_t now) {
    Core *core = valid(state);
    if (!core || !core->entered || core->owner != thread_id() || !core->writable || core->read_only
        || now < core->beat || now - core->beat > 2200 || now < core->now) return -1;
    core->writes++;
    return 0;
}
uint64_t silph_core_issue(void *state, uint64_t encounter, uint64_t sent_at, uint32_t source) {
    Core *core = valid(state);
    if (!core || !encounter || core->pending_sequence || source < 1 || source > 2 || core->sequence == UINT64_MAX) return 0;
    core->pending_sequence = ++core->sequence;core->pending_encounter = encounter;
    core->pending_sent = sent_at;core->pending_source = source;
    return core->pending_sequence;
}
int silph_core_abandon(void *state, uint64_t sequence, uint64_t encounter, uint64_t sent_at) {
    Core *core = valid(state);
    if (!core || !sequence || core->pending_sequence != sequence || core->pending_encounter != encounter || core->pending_sent != sent_at) return -1;
    core->pending_sequence = 0;
    return 0;
}
int silph_core_settle(void *state, const uint64_t *proof, uint32_t length) {
    Core *core = valid(state);
    if (!core || !proof || length != 6) return -1;
    if (!proof[0] || core->pending_sequence != proof[0]) return 0;
    core->pending_sequence = 0;
    if (proof[1] != core->pending_encounter || proof[2] != core->pending_sent
        || proof[3] < proof[2] || proof[3] - proof[2] > 18000 || !proof[4] || proof[5] != 15) return 0;
    Entry *entry = find(core, proof[4]);
    if (!entry || entry->id) return 0;
    if (core->count >= core->capacity) return -2;
    entry->id = proof[4];entry->encounter = proof[1];entry->state = CAPTURED;entry->source = core->pending_source;
    core->count++;
    return 1;
}
int silph_core_authorize(void *state, uint64_t id, uint64_t encounter) {
    Core *core = valid(state);
    if (!core || !encounter) return 0;
    Entry *entry = find(core, id);
    return entry && entry->id == id && entry->state == CAPTURED && entry->encounter == encounter
        && (!core->limit || core->confirmed < core->limit);
}
int silph_core_begin(void *state, uint64_t id, uint64_t encounter) {
    if (!silph_core_authorize(state, id, encounter)) return -1;
    Core *core = state;
    find(core, id)->state = SENT;
    return 0;
}
int silph_core_finish(void *state, uint64_t id, uint32_t confirmed) {
    Core *core = valid(state);
    if (!core || confirmed > 1) return -1;
    Entry *entry = find(core, id);
    if (!entry || entry->id != id || entry->state == RELEASED || (confirmed && entry->state != SENT)) return -1;
    if (confirmed) core->confirmed++;
    entry->state = RELEASED;
    return 0;
}
int silph_core_metrics(void *state, uint64_t *values, uint32_t length) {
    Core *core = valid(state);
    if (!core || !values || length != 6) return -1;
    values[0] = core->ticks;values[1] = core->writes;values[2] = core->count;
    values[3] = core->confirmed;values[4] = core->pending_sequence != 0;values[5] = core->bytes;
    return 0;
}
int silph_core_close(void *state) {
    Core *core = valid(state);
    if (!core || (core->entered && core->owner != thread_id())) return -1;
    clear_bytes(core, core->bytes);
    return 0;
}
