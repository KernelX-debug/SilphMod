#ifndef SILPH_MANAGED_READ_H
#define SILPH_MANAGED_READ_H
#include "managed.h"

enum { SILPH_READ_VOID=1, SILPH_READ_BOOL=2, SILPH_READ_I32=8, SILPH_READ_I64=10, SILPH_READ_U64=11,
    SILPH_READ_F32=12, SILPH_READ_F64=13, SILPH_READ_REFERENCE=18, SILPH_READ_ENUM32=64, SILPH_READ_BOXED=65 };
typedef struct { uint64_t bits;SilphManagedRef reference; } SilphReadValue;
typedef struct { uint32_t kind, reserved;SilphReadValue value; } SilphReadArgument;

SILPH_MANAGED_EXPORT size_t silph_read_size(void);
SILPH_MANAGED_EXPORT int silph_read_open(void *, size_t, void *, const SilphManagedApi *, SilphManagedResolver, void *);
SILPH_MANAGED_EXPORT int silph_read_field(void *, SilphManagedRef, const char *, uint32_t, SilphReadValue *);
SILPH_MANAGED_EXPORT int silph_read_field_type(void *, SilphManagedRef, const char *, const char *, SilphManagedRef *);
SILPH_MANAGED_EXPORT int silph_read_is_type(void *, SilphManagedRef, const char *, const char *);
SILPH_MANAGED_EXPORT int silph_read_call(void *, SilphManagedRef, const char *, uint32_t, const SilphReadArgument *, uint32_t, SilphReadValue *);
SILPH_MANAGED_EXPORT int silph_read_type_object(void *, const char *, const char *, const char *, SilphManagedRef *);
SILPH_MANAGED_EXPORT int silph_read_utf8(void *, SilphManagedRef, char *, uint32_t, uint32_t *);
SILPH_MANAGED_EXPORT int silph_read_close(void *);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
typedef struct {uint64_t hits,misses;} SilphReadCacheMetrics;
int silph_read_cache_metrics(void *,SilphReadCacheMetrics *);
#endif
#endif
