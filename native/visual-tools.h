#ifndef SILPH_VISUAL_TOOLS_H
#define SILPH_VISUAL_TOOLS_H
#include "managed.h"
#include <stdint.h>
typedef bool (*SilphVisualAllowed)(void *);
typedef int (*SilphVisualRead)(uintptr_t,void *,size_t);
typedef bool (*SilphVisualAddress)(uintptr_t,void *);
typedef struct {uint32_t available,failed;uint64_t iv_reads,name_updates,created,released,remaining;} SilphVisualMetrics;
size_t silph_visual_size(void);
int silph_visual_open(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,uint32_t,SilphVisualAllowed,void *,SilphVisualAddress);
int silph_visual_stage(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,uint32_t,SilphVisualAllowed,void *,SilphVisualAddress);
int silph_visual_adopt(void *,uint32_t,SilphVisualAllowed,void *,uint32_t);
int silph_visual_offset(const SilphManagedApi *,SilphManagedResolver,void *,SilphVisualRead,uintptr_t,uintptr_t,uint32_t *);
bool silph_visual_busy(void *);
bool silph_visual_hundo(int,int,int,bool);
int silph_visual_close(void *,SilphVisualMetrics *);
int silph_visual_metrics(void *,SilphVisualMetrics *);
void silph_visual_prime(void *,void *);
int silph_visual_iv_label(int,int,int,bool,char *,size_t);
#endif
