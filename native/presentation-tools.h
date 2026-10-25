#ifndef SILPH_PRESENTATION_TOOLS_H
#define SILPH_PRESENTATION_TOOLS_H
#include "visual-tools.h"
enum {SILPH_PRESENT_SCENES=1024,SILPH_PRESENT_MANUAL=2048,SILPH_PRESENT_GIFTS=4096};
typedef struct {
    uint32_t available,failed,stage,remaining_hooks;
    uint64_t scenes,eggs,gifts,waits,selections,batches,created,released,remaining;
    uint64_t open_drains;
    uint32_t last_scene,pending;
    uint32_t scene_supported,scene_errors;
    uint64_t scene_retries,scene_timeouts;
} SilphPresentationMetrics;
size_t silph_presentation_size(void);
int silph_presentation_open(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,uint32_t,SilphVisualAllowed,void *,SilphVisualAddress);
int silph_presentation_prime(void *,void *,void *);
void silph_presentation_map(void *,void *,uint64_t);
void silph_presentation_drain(void *,uint64_t);
bool silph_presentation_busy(void *);
int silph_presentation_metrics(void *,SilphPresentationMetrics *);
int silph_presentation_close(void *,SilphPresentationMetrics *);
#endif
