#ifndef SILPH_MANAGED_H
#define SILPH_MANAGED_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SILPH_MANAGED_ABI 1u
#define SILPH_MANAGED_MAX_REFS 1024u
#define SILPH_MANAGED_MAX_METHODS 32u
#define SILPH_MANAGED_EXPORT __attribute__((visibility("default")))

typedef struct {
    void *(*domain_get)(void);
    void *(*thread_current)(void);
    void *(*thread_attach)(void *);
    void (*thread_detach)(void *);
    uintptr_t (*gchandle_new)(void *, bool);
    uintptr_t (*gchandle_new_weakref)(void *, bool);
    void *(*gchandle_get_target)(uintptr_t);
    void (*gchandle_free)(uintptr_t);
    void *(*object_get_class)(void *);
    void *(*object_unbox)(void *);
    const void *(*class_get_type)(void *);
    const void *(*class_get_methods)(void *, void **);
    const char *(*method_get_name)(const void *);
    uint32_t (*method_get_param_count)(const void *);
    const void *(*method_get_return_type)(const void *);
    uint32_t (*method_get_flags)(const void *, uint32_t *);
    bool (*method_is_generic)(const void *);
    bool (*method_is_inflated)(const void *);
    int (*type_get_type)(const void *);
    void *(*runtime_invoke)(const void *, void *, void **, void **);
    void *(*string_new_len)(const char *, uint32_t);
    int32_t (*string_length)(void *);
} SilphManagedApi;

typedef void *(*SilphManagedResolver)(const char *, void *);
typedef struct { uint64_t epoch, serial; } SilphManagedRef;
typedef struct { uint64_t epoch, serial; } SilphManagedMethod;
typedef struct {
    uint64_t refs_created, refs_released, invocations, managed_exceptions;
    uint32_t live_refs, methods, owns_attachment, closed;
} SilphManagedMetrics;

enum {
    SILPH_MANAGED_OK = 0, SILPH_MANAGED_ARGUMENT = 1, SILPH_MANAGED_ABI_MISMATCH = 2,
    SILPH_MANAGED_STATE = 3, SILPH_MANAGED_THREAD = 4, SILPH_MANAGED_NOT_READY = 5,
    SILPH_MANAGED_STALE = 6, SILPH_MANAGED_CAPACITY = 7, SILPH_MANAGED_REFERENCE = 8,
    SILPH_MANAGED_CONTRACT = 9, SILPH_MANAGED_EXCEPTION = 10, SILPH_MANAGED_BUSY = 11
};

SILPH_MANAGED_EXPORT uint32_t silph_managed_abi(void);
SILPH_MANAGED_EXPORT int silph_managed_api_load(SilphManagedApi *, SilphManagedResolver, void *);
SILPH_MANAGED_EXPORT size_t silph_managed_size(uint32_t);
SILPH_MANAGED_EXPORT int silph_managed_open(void *, size_t, const SilphManagedApi *, uint32_t, uint32_t);
SILPH_MANAGED_EXPORT int silph_managed_retain(void *, void *, bool, SilphManagedRef *);
SILPH_MANAGED_EXPORT int silph_managed_target(void *, SilphManagedRef, void **);
SILPH_MANAGED_EXPORT int silph_managed_release(void *, SilphManagedRef);
SILPH_MANAGED_EXPORT int silph_managed_ascii(void *, const char *, uint32_t, SilphManagedRef *);
SILPH_MANAGED_EXPORT int silph_managed_getter_i32(void *, SilphManagedRef, const char *, SilphManagedMethod *);
SILPH_MANAGED_EXPORT int silph_managed_invoke_i32(void *, SilphManagedMethod, SilphManagedRef, int32_t *);
SILPH_MANAGED_EXPORT int silph_managed_metrics(void *, SilphManagedMetrics *);
SILPH_MANAGED_EXPORT int silph_managed_close(void *, SilphManagedMetrics *);
#endif
