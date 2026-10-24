#define _GNU_SOURCE
#include "managed.h"
#include <stdatomic.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

_Static_assert(sizeof(uintptr_t) == 8, "The inspected GCHandle ABI requires 64-bit pointers");
#define MAGIC UINT32_C(0x534d4231)
#define CLOSED UINT32_C(0x534d4243)
#define UNCERTAIN UINT32_C(0x534d4255)
#define I4 8
#define METHOD_STATIC UINT32_C(0x10)
typedef struct { uint64_t serial;uintptr_t token;void *original; } Reference;
typedef struct { const void *method;void *klass;uint64_t serial; } Getter;
typedef struct {
    atomic_uint gate;
    uint32_t magic, owner, capacity, live, method_count, owns_attachment;
    size_t bytes;
    uint64_t epoch, sequence;
    void *domain, *thread;
    SilphManagedApi api;
    SilphManagedMetrics metrics;
    Getter getters[SILPH_MANAGED_MAX_METHODS];
    Reference refs[];
} Managed;
static atomic_uint_fast64_t epochs = ATOMIC_VAR_INIT(1);
static uint64_t next_epoch(void) {
    uint_fast64_t value = atomic_load_explicit(&epochs, memory_order_relaxed);
    while (value && value < UINT64_MAX) {
        if (atomic_compare_exchange_weak_explicit(&epochs, &value, value + 1, memory_order_relaxed, memory_order_relaxed)) return value;
    }
    return 0;
}
static uint32_t tid(void) { return (uint32_t)syscall(__NR_gettid); }
static void zero(void *value, size_t bytes) {
    volatile unsigned char *p = value;
    while (bytes--) *p++ = 0;
}
uint32_t silph_managed_abi(void) { return SILPH_MANAGED_ABI; }
size_t silph_managed_size(uint32_t capacity) {
    return capacity && capacity <= SILPH_MANAGED_MAX_REFS ? sizeof(Managed) + capacity * sizeof(Reference) : 0;
}
static int complete(const SilphManagedApi *api) {
#define REQUIRED(name) if (!api->name) return 0
    if (!api) return 0;
    REQUIRED(domain_get);REQUIRED(thread_current);REQUIRED(thread_attach);REQUIRED(thread_detach);
    REQUIRED(gchandle_new);REQUIRED(gchandle_new_weakref);REQUIRED(gchandle_get_target);REQUIRED(gchandle_free);
    REQUIRED(object_get_class);REQUIRED(object_unbox);REQUIRED(class_get_type);REQUIRED(class_get_methods);
    REQUIRED(method_get_name);REQUIRED(method_get_param_count);REQUIRED(method_get_return_type);REQUIRED(method_get_flags);
    REQUIRED(method_is_generic);REQUIRED(method_is_inflated);REQUIRED(type_get_type);REQUIRED(runtime_invoke);
    REQUIRED(string_new_len);REQUIRED(string_length);
#undef REQUIRED
    return 1;
}
int silph_managed_api_load(SilphManagedApi *output, SilphManagedResolver resolver, void *context) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (!resolver) return SILPH_MANAGED_ARGUMENT;
    SilphManagedApi api = {0};
#define LOAD(name) do { void *symbol = resolver("il2cpp_" #name, context); \
    if (!symbol) return SILPH_MANAGED_ABI_MISMATCH; \
    _Static_assert(sizeof(api.name) == sizeof(symbol), "Incompatible function pointer size"); \
    memcpy(&api.name, &symbol, sizeof(symbol)); } while (0)
    LOAD(domain_get);LOAD(thread_current);LOAD(thread_attach);LOAD(thread_detach);
    LOAD(gchandle_new);LOAD(gchandle_new_weakref);LOAD(gchandle_get_target);LOAD(gchandle_free);
    LOAD(object_get_class);LOAD(object_unbox);LOAD(class_get_type);LOAD(class_get_methods);
    LOAD(method_get_name);LOAD(method_get_param_count);LOAD(method_get_return_type);LOAD(method_get_flags);
    LOAD(method_is_generic);LOAD(method_is_inflated);LOAD(type_get_type);LOAD(runtime_invoke);
    LOAD(string_new_len);LOAD(string_length);
#undef LOAD
    *output = api;
    return SILPH_MANAGED_OK;
}
static int lock(Managed *state) {
    if (!state) return SILPH_MANAGED_ARGUMENT;
    unsigned int expected = 0;
    return atomic_compare_exchange_strong_explicit(&state->gate, &expected, 1, memory_order_acquire, memory_order_relaxed)
        ? SILPH_MANAGED_OK : SILPH_MANAGED_BUSY;
}
static void unlock(Managed *state) { atomic_store_explicit(&state->gate, 0, memory_order_release); }
static int valid(Managed *state) {
    if (state->magic != MAGIC || state->bytes != silph_managed_size(state->capacity)) return SILPH_MANAGED_STATE;
    if (state->owner != tid()) return SILPH_MANAGED_THREAD;
    if (state->api.domain_get() != state->domain || state->api.thread_current() != state->thread) return SILPH_MANAGED_STALE;
    return SILPH_MANAGED_OK;
}
static int enter(Managed *state) {
    int status = lock(state);
    if (status) return status;
    status = valid(state);
    if (status) unlock(state);
    return status;
}
static Reference *find(Managed *state, SilphManagedRef ref) {
    if (!ref.serial || ref.epoch != state->epoch) return NULL;
    for (uint32_t i = 0; i < state->capacity; i++) if (state->refs[i].serial == ref.serial) return &state->refs[i];
    return NULL;
}
static int target(Managed *state, SilphManagedRef ref, void **output) {
    Reference *entry = find(state, ref);
    if (!entry || !entry->token) return SILPH_MANAGED_REFERENCE;
    void *object = state->api.gchandle_get_target(entry->token);
    if (!object || object != entry->original) return SILPH_MANAGED_REFERENCE;
    *output = object;
    return SILPH_MANAGED_OK;
}
static int retain(Managed *state, void *object, bool weak, SilphManagedRef *output) {
    if (state->live >= state->capacity || state->sequence == UINT64_MAX) return SILPH_MANAGED_CAPACITY;
    Reference *slot = NULL;
    for (uint32_t i = 0; i < state->capacity; i++) if (!state->refs[i].token) { slot = &state->refs[i];break; }
    if (!slot) return SILPH_MANAGED_CAPACITY;
    uintptr_t token = weak ? state->api.gchandle_new_weakref(object, false) : state->api.gchandle_new(object, true);
    if (!token) return SILPH_MANAGED_REFERENCE;
    if (state->api.gchandle_get_target(token) != object) {
        state->api.gchandle_free(token);
        return SILPH_MANAGED_REFERENCE;
    }
    slot->token = token;slot->original = object;slot->serial = ++state->sequence;
    state->live++;state->metrics.refs_created++;
    output->epoch = state->epoch;output->serial = slot->serial;
    return SILPH_MANAGED_OK;
}
int silph_managed_open(void *memory, size_t bytes, const SilphManagedApi *api, uint32_t capacity, uint32_t gc_bits) {
    if (!memory || !silph_managed_size(capacity) || bytes != silph_managed_size(capacity)) return SILPH_MANAGED_ARGUMENT;
    uint32_t magic;
    memcpy(&magic, (const unsigned char *)memory + offsetof(Managed, magic), sizeof(magic));
    if (magic == MAGIC || magic == UNCERTAIN) return SILPH_MANAGED_STATE;
    if (gc_bits != 64 || !complete(api)) return SILPH_MANAGED_ABI_MISMATCH;
    SilphManagedApi loaded = *api;
    void *domain = loaded.domain_get();
    if (!domain) return SILPH_MANAGED_NOT_READY;
    void *thread = loaded.thread_current();
    bool owned = thread == NULL;
    if (owned) thread = loaded.thread_attach(domain);
    if (!thread) return SILPH_MANAGED_NOT_READY;
    if (loaded.thread_current() != thread || loaded.domain_get() != domain) {
        if (owned) loaded.thread_detach(thread);
        return SILPH_MANAGED_STALE;
    }
    uint64_t epoch = next_epoch();
    if (!epoch) {
        if (owned) loaded.thread_detach(thread);
        return SILPH_MANAGED_STATE;
    }
    memset(memory, 0, bytes);
    Managed *state = memory;atomic_init(&state->gate, 0);
    state->magic = MAGIC;state->owner = tid();state->capacity = capacity;state->bytes = bytes;
    state->epoch = epoch;state->api = loaded;state->domain = domain;state->thread = thread;
    state->owns_attachment = owned;
    return SILPH_MANAGED_OK;
}
int silph_managed_retain(void *memory, void *object, bool weak, SilphManagedRef *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (!object) return SILPH_MANAGED_ARGUMENT;
    Managed *state = memory;int status = enter(state);if (status) return status;
    status = retain(state, object, weak, output);unlock(state);return status;
}
int silph_managed_target(void *memory, SilphManagedRef ref, void **output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    *output = NULL;
    Managed *state = memory;int status = enter(state);if (status) return status;
    status = target(state, ref, output);unlock(state);return status;
}
int silph_managed_release(void *memory, SilphManagedRef ref) {
    Managed *state = memory;int status = enter(state);if (status) return status;
    Reference *slot = find(state, ref);
    if (!slot || !slot->token) status = SILPH_MANAGED_REFERENCE;
    else {
        state->api.gchandle_free(slot->token);zero(slot, sizeof(*slot));
        state->live--;state->metrics.refs_released++;
    }
    unlock(state);return status;
}
int silph_managed_ascii(void *memory, const char *text, uint32_t length, SilphManagedRef *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (!text || !length || length > 512) return SILPH_MANAGED_ARGUMENT;
    for (uint32_t i = 0; i < length; i++) if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return SILPH_MANAGED_ARGUMENT;
    Managed *state = memory;int status = enter(state);if (status) return status;
    if (state->live >= state->capacity) status = SILPH_MANAGED_CAPACITY;
    else {
        void *value = state->api.string_new_len(text, length);
        status = value ? retain(state, value, false, output) : SILPH_MANAGED_REFERENCE;
        if (!status) {
            void *held = NULL;
            status = target(state, *output, &held);
            if (!status && state->api.string_length(held) != (int32_t)length) status = SILPH_MANAGED_CONTRACT;
            if (status) {
                Reference *slot = find(state, *output);
                if (slot && slot->token) {
                    state->api.gchandle_free(slot->token);zero(slot, sizeof(*slot));
                    state->live--;state->metrics.refs_released++;
                }
                memset(output, 0, sizeof(*output));
            }
        }
    }
    unlock(state);return status;
}
static int contract(Managed *state, const void *method) {
    if (!method || state->api.method_get_param_count(method) != 0
        || state->api.method_is_generic(method) || state->api.method_is_inflated(method)) return 0;
    uint32_t flags = 0;
    const void *type = state->api.method_get_return_type(method);
    return type && state->api.type_get_type(type) == I4
        && !(state->api.method_get_flags(method, &flags) & METHOD_STATIC);
}
int silph_managed_getter_i32(void *memory, SilphManagedRef ref, const char *name, SilphManagedMethod *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (!name || strnlen(name, 129) > 128 || strncmp(name, "get_", 4) || !name[4]) return SILPH_MANAGED_ARGUMENT;
    Managed *state = memory;int status = enter(state);if (status) return status;
    void *object = NULL;status = target(state, ref, &object);
    void *klass = status ? NULL : state->api.object_get_class(object);
    if (!status && !klass) status = SILPH_MANAGED_REFERENCE;
    const void *found = NULL;void *iterator = NULL;
    if (!status) {
        for (uint32_t i = 0; ; i++) {
            if (i >= 1024) { status = SILPH_MANAGED_CONTRACT;break; }
            const void *method = state->api.class_get_methods(klass, &iterator);
            if (!method) break;
            const char *actual = state->api.method_get_name(method);
            if (!actual || strnlen(actual, 129) > 128) { status = SILPH_MANAGED_CONTRACT;break; }
            if (!strcmp(name, actual)) {
                if (found || !contract(state, method)) { status = SILPH_MANAGED_CONTRACT;break; }
                found = method;
            }
        }
        if (!status && !found) status = SILPH_MANAGED_CONTRACT;
    }
    if (!status) {
        Getter *slot = NULL;
        for (uint32_t i = 0; i < state->method_count; i++) if (state->getters[i].method == found && state->getters[i].klass == klass) slot = &state->getters[i];
        if (!slot) {
            if (state->method_count == SILPH_MANAGED_MAX_METHODS || state->sequence == UINT64_MAX) status = SILPH_MANAGED_CAPACITY;
            else { slot = &state->getters[state->method_count++];slot->method = found;slot->klass = klass;slot->serial = ++state->sequence; }
        }
        if (!status) { output->epoch = state->epoch;output->serial = slot->serial; }
    }
    unlock(state);return status;
}
int silph_managed_invoke_i32(void *memory, SilphManagedMethod method, SilphManagedRef ref, int32_t *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    *output = 0;
    Managed *state = memory;int status = enter(state);if (status) return status;
    Getter *getter = NULL;
    if (method.epoch == state->epoch && method.serial)
        for (uint32_t i = 0; i < state->method_count; i++) if (state->getters[i].serial == method.serial) getter = &state->getters[i];
    void *object = NULL;
    if (!getter) status = SILPH_MANAGED_CONTRACT;
    else status = target(state, ref, &object);
    if (!status && state->live >= state->capacity) status = SILPH_MANAGED_CAPACITY;
    if (!status && (state->api.object_get_class(object) != getter->klass || !contract(state, getter->method))) status = SILPH_MANAGED_CONTRACT;
    if (!status) {
        void *exception = NULL;
        void *value = state->api.runtime_invoke(getter->method, object, NULL, &exception);
        state->metrics.invocations++;
        if (exception) { state->metrics.managed_exceptions++;status = SILPH_MANAGED_EXCEPTION; }
        else if (!value) status = SILPH_MANAGED_CONTRACT;
        else {
            SilphManagedRef boxed = {0};status = retain(state, value, false, &boxed);
            if (!status) {
                void *held = NULL;status = target(state, boxed, &held);
                void *klass = status ? NULL : state->api.object_get_class(held);
                const void *type = klass ? state->api.class_get_type(klass) : NULL;
                void *scalar = NULL;
                if (!status && (!type || state->api.type_get_type(type) != I4)) status = SILPH_MANAGED_CONTRACT;
                if (!status && !(scalar = state->api.object_unbox(held))) status = SILPH_MANAGED_CONTRACT;
                if (!status) memcpy(output, scalar, sizeof(*output));
                Reference *slot = find(state, boxed);
                state->api.gchandle_free(slot->token);zero(slot, sizeof(*slot));
                state->live--;state->metrics.refs_released++;
            }
        }
    }
    unlock(state);return status;
}
static void metrics(Managed *state, SilphManagedMetrics *output) {
    *output = state->metrics;output->live_refs = state->live;output->methods = state->method_count;
    output->owns_attachment = state->owns_attachment;output->closed = state->magic == CLOSED;
}
int silph_managed_metrics(void *memory, SilphManagedMetrics *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    Managed *state = memory;int status = lock(state);if (status) return status;
    if (state->magic == CLOSED) metrics(state, output);
    else { status = valid(state);if (!status) metrics(state, output); }
    unlock(state);return status;
}
int silph_managed_close(void *memory, SilphManagedMetrics *output) {
    if (!output) return SILPH_MANAGED_ARGUMENT;
    memset(output, 0, sizeof(*output));
    Managed *state = memory;int status = lock(state);if (status) return status;
    if (state->owner != tid()) status = SILPH_MANAGED_THREAD;
    else if (state->magic != CLOSED) {
        status = valid(state);
        if (!status) {
            for (uint32_t i = 0; i < state->capacity; i++) if (state->refs[i].token) {
                state->api.gchandle_free(state->refs[i].token);zero(&state->refs[i], sizeof(Reference));
                state->live--;state->metrics.refs_released++;
            }
            if (state->owns_attachment) {
                state->api.thread_detach(state->thread);
                if (state->api.thread_current()) { status = SILPH_MANAGED_STALE;state->magic = UNCERTAIN; }
            }
            if (!status) {
                zero(&state->api, sizeof(state->api));zero(state->getters, sizeof(state->getters));
                state->domain = NULL;state->thread = NULL;state->magic = CLOSED;
            }
        }
    }
    if (state->magic == MAGIC || state->magic == CLOSED || state->magic == UNCERTAIN) metrics(state, output);
    unlock(state);return status;
}
