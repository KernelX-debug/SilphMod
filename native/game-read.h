#ifndef SILPH_GAME_READ_H
#define SILPH_GAME_READ_H
#include "managed-read.h"
#include "transfer-policy.h"

typedef struct {
    uint64_t samples,account_hi,account_lo,first_encounter;
    uint32_t pokemon_count,max_pokemon,item_count,max_items,map_pokemon,map_stops,candidates_read,first_species;
    uint32_t items[8];
} SilphGameSnapshot;
typedef struct { SilphManagedRef object;uint64_t encounter;uint32_t species,reserved;double meters; } SilphGameTarget;
typedef struct {
    double latitude,longitude,player_latitude,player_longitude,meters;
    uint64_t expires_ms;
    uint32_t incident_type,active,complete,scanned,scan_complete,matches;
    char fort[161],incident[161];
} SilphGameStop;
typedef bool (*SilphStopPredicate)(void *,const char *);
typedef bool (*SilphPokemonPredicate)(void *,uint64_t,uint32_t);
size_t silph_game_read_size(void);
int silph_game_read_open(void *,size_t,void *,const SilphManagedApi *,void *,void *);
int silph_game_read_compact(void *);
int silph_game_read_sample(void *,void *,SilphGameSnapshot *);
int silph_game_read_poll(void *,void *,bool,SilphGameSnapshot *);
int silph_game_read_refresh(void *,bool,SilphGameSnapshot *);
int silph_game_read_close(void *);
unsigned silph_game_read_stage(void *);
uint64_t silph_game_read_samples(void *);
int silph_game_read_nearest(void *,double,SilphGameTarget *);
int silph_game_read_select(void *,double,SilphPokemonPredicate,void *,SilphGameTarget *);
int silph_game_read_distance(void *,SilphManagedRef,double *);
int silph_game_read_position(void *,double *,double *);
int silph_game_read_stop(void *,double,uint32_t,SilphStopPredicate,void *,SilphGameStop *);
int silph_game_read_stop_at(void *,double,double,double,double,SilphGameStop *);
int silph_game_read_item_bag(void *,SilphManagedRef *);
int silph_game_read_item(void *,int32_t,uint32_t *);
int silph_game_read_owned(void *,uint64_t,SilphManagedRef *);
int silph_game_read_rpc_handler(void *,SilphManagedRef *);
int silph_game_read_service(void *,uint32_t,SilphManagedRef *);
int silph_game_read_gift_service(void *,uint32_t,SilphManagedRef *);
int silph_game_read_map_count(void *,uint32_t *);
int silph_game_read_research_service(void *,uint32_t,SilphManagedRef *);
int silph_game_read_transfer_data(void *,uint64_t,SilphTransferInput *,SilphManagedRef *,SilphManagedRef *);
#endif
