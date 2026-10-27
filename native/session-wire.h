#ifndef SILPH_SESSION_WIRE_H
#define SILPH_SESSION_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define SILPH_CONFIG_BYTES 1024u
#define SILPH_SNAPSHOT_BYTES 256u
#define SILPH_CONFIG_MAGIC UINT64_C(0x3147464348504c53)
#define SILPH_SNAPSHOT_MAGIC UINT64_C(0x3154415348504c53)
enum {SILPH_MODE_FARMER=1,SILPH_MODE_KECLEON=2,SILPH_MODE_ROCKET=3};
enum {SILPH_CFG_CATCH=1,SILPH_CFG_SPIN=2,SILPH_CFG_CLEAN=4,SILPH_CFG_TRANSFER=8,SILPH_CFG_BERRIES=16,SILPH_CFG_CURVE=32,SILPH_CFG_READONLY=64,SILPH_CFG_TRAINER_NAME=128,SILPH_CFG_IV_NAMES=256,SILPH_CFG_FIX_LOCATION=512,SILPH_CFG_SCENES=1024,SILPH_CFG_MANUAL=2048,SILPH_CFG_GIFTS=4096,SILPH_CFG_BUDDY=8192,SILPH_CFG_OPEN_GIFTS=16384,SILPH_CFG_SEND_GIFTS=32768,SILPH_CFG_RECOMMENDED_TEAM=65536,SILPH_CFG_EVENT_ACTIONS=131072};
enum {SILPH_WIRE_OK=0,SILPH_WIRE_ARGUMENT=1,SILPH_WIRE_FORMAT=2,SILPH_WIRE_CRC=3};
typedef struct {
    uint64_t ticks,session_hi,session_lo;
    uint32_t pid,mode,flags,quality,delay_ms,keep,max_catches,max_spins,max_encounters,berry_count;
    uint32_t berries[5],limit_count;
    struct {uint32_t item,floor;} limits[64];
    uint8_t species[256];
    uint32_t filters,duration_ms;
} SilphSessionConfig;
typedef struct {
    uint64_t ticks,session_hi,session_lo,sequence,account_hi,account_lo;
    uint64_t caught,fled,spun,discarded,transferred,encounters,shinies,berries,throws,xp,stardust,encounter,captured;
    uint64_t candidate_request;
    double latitude,longitude;
    uint32_t mode,state,error,reason,flags,pid,candidate_state,rewards_available;
    uint32_t visual_available,visual_failed,research_status,location_status;
    uint64_t iv_reads,name_updates;
} SilphSessionSnapshot;
uint32_t silph_wire_crc(const void *,size_t);
int silph_config_validate(const SilphSessionConfig *);
int silph_config_encode(const SilphSessionConfig *,void *,size_t);
int silph_config_decode(const void *,size_t,SilphSessionConfig *);
int silph_snapshot_encode(const SilphSessionSnapshot *,void *,size_t);
int silph_snapshot_decode(const void *,size_t,SilphSessionSnapshot *);
#endif
