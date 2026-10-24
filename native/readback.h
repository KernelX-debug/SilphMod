#ifndef SILPH_READBACK_H
#define SILPH_READBACK_H

#include <stddef.h>
#include <stdint.h>

#define SILPH_READBACK_ABI 1u
#define SILPH_READBACK_MAX_ITEMS 128u

#if defined(__GNUC__)
#define SILPH_EXPORT __attribute__((visibility("default")))
#else
#define SILPH_EXPORT
#endif

typedef int32_t (*SilphItemCount)(void *, int32_t, const void *);
typedef void *(*SilphObjectClass)(void *);

enum SilphReadbackStatus {
    SILPH_READBACK_OK = 0,
    SILPH_READBACK_ARGUMENT = 1,
    SILPH_READBACK_STATE = 2,
    SILPH_READBACK_THREAD = 3,
    SILPH_READBACK_CLASS = 4,
    SILPH_READBACK_COUNT = 5
};

SILPH_EXPORT uint32_t silph_readback_abi(void);
SILPH_EXPORT size_t silph_readback_size(void);
SILPH_EXPORT int silph_readback_open(void *, size_t, SilphItemCount, const void *, void *, SilphObjectClass);
SILPH_EXPORT int silph_readback_counts(void *, void *, const int32_t *, uint32_t, int32_t *, uint32_t);
SILPH_EXPORT int silph_readback_metrics(void *, uint64_t *, uint32_t);
SILPH_EXPORT int silph_readback_close(void *);

#endif
