#ifndef SILPH_CORE_H
#define SILPH_CORE_H
#include <stddef.h>
#include <stdint.h>
#include "readback.h"

#define SILPH_CORE_ABI 1u
#define SILPH_CORE_MAX_CAPTURES 50000u
SILPH_EXPORT uint32_t silph_core_abi(void);
SILPH_EXPORT uint32_t silph_core_size(uint32_t);
SILPH_EXPORT int silph_core_open(void *, uint32_t, uint32_t, uint32_t);
SILPH_EXPORT int silph_core_enter(void *, uint64_t, uint64_t, uint32_t, uint32_t);
SILPH_EXPORT int silph_core_leave(void *);
SILPH_EXPORT int silph_core_write(void *, uint64_t);
SILPH_EXPORT uint64_t silph_core_issue(void *, uint64_t, uint64_t, uint32_t);
SILPH_EXPORT int silph_core_abandon(void *, uint64_t, uint64_t, uint64_t);
SILPH_EXPORT int silph_core_settle(void *, const uint64_t *, uint32_t);
SILPH_EXPORT int silph_core_authorize(void *, uint64_t, uint64_t);
SILPH_EXPORT int silph_core_begin(void *, uint64_t, uint64_t);
SILPH_EXPORT int silph_core_finish(void *, uint64_t, uint32_t);
SILPH_EXPORT int silph_core_metrics(void *, uint64_t *, uint32_t);
SILPH_EXPORT int silph_core_close(void *);
#endif
