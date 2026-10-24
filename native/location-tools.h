#ifndef SILPH_LOCATION_TOOLS_H
#define SILPH_LOCATION_TOOLS_H
#include <stdbool.h>
#include <stdint.h>
typedef bool (*SilphLocationAllowed)(void *);
typedef struct {
    uint32_t state,error,stage,methods,hooks,remaining,global_refs,cleanup,inflight;
    uint32_t read_method,read_errno,mem_errno,protect_errno,restore_errno,hook_index,code_protection;
    uint64_t calls,changed,legacy_calls,modern_calls,exception_bypass,object_bypass;
} SilphLocationMetrics;
int silph_location_open(void *,SilphLocationAllowed,void *);
void silph_location_metrics(SilphLocationMetrics *);
int silph_location_close(void);
#endif
