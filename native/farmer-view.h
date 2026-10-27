#ifndef SILPH_FARMER_VIEW_H
#define SILPH_FARMER_VIEW_H
#include "visual-tools.h"
enum {SILPH_VIEW_BLOCKED=0,SILPH_VIEW_MAP=1,SILPH_VIEW_POKEMON=2,SILPH_VIEW_ITEMS=3,SILPH_VIEW_PROFILE=4,SILPH_VIEW_POKEMON_INFO=5,SILPH_VIEW_STORE=6,SILPH_VIEW_POKEDEX=7};
typedef struct {
    uint32_t available,kind,stage,hooks,remaining;
    uint64_t updates,pumps,menu_pumps,skipped,max_ns,created,released;
    char name[64];
} SilphFarmerViewMetrics;
typedef void (*SilphFarmerPump)(void *,bool,uint32_t);
size_t silph_farmer_view_size(void);
int silph_farmer_view_open(void *,size_t,const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,void *,SilphVisualAllowed,void *,SilphVisualAddress,SilphFarmerPump);
bool silph_farmer_view_ready(void *);
void silph_farmer_view_interval(void *,uint32_t);
void silph_farmer_view_continuation(void *,bool);
void silph_farmer_view_rocket(void *,bool);
void silph_farmer_view_kecleon(void *,bool);
int silph_farmer_view_metrics(void *,SilphFarmerViewMetrics *);
int silph_farmer_view_close(void *,SilphFarmerViewMetrics *);
#endif
