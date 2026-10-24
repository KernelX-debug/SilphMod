#ifndef SILPH_VISUAL_TOOLS_H
#define SILPH_VISUAL_TOOLS_H
#include "managed.h"
#include <stdint.h>
typedef bool (*SilphVisualAllowed)(void *);
typedef bool (*SilphVisualAddress)(uintptr_t,void *);
typedef struct {uint32_t available,failed;uint64_t iv_reads,name_updates,created,released,remaining;} SilphVisualMetrics;
size_t silph_visual_size(void);
int silph_visual_open(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,uint32_t,SilphVisualAllowed,void *,SilphVisualAddress);
int silph_visual_close(void *,SilphVisualMetrics *);
int silph_visual_metrics(void *,SilphVisualMetrics *);
void silph_visual_prime(void *,void *);
int silph_visual_iv_label(int,int,int,bool,char *,size_t);
#endif
