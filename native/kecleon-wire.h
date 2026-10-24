#ifndef SILPH_KECLEON_WIRE_H
#define SILPH_KECLEON_WIRE_H
#include "session-wire.h"
#include "kecleon-plan.h"
#define SILPH_KEC_COMMAND_BYTES 512u
#define SILPH_KEC_EVENT_BYTES 640u
#define SILPH_KEC_BUNDLE_BYTES (SILPH_SNAPSHOT_BYTES+SILPH_KEC_EVENT_BYTES)
#define SILPH_KEC_COMMAND_MAGIC UINT64_C(0x314d434b48504c53)
#define SILPH_KEC_EVENT_MAGIC UINT64_C(0x3156454b48504c53)
enum {SILPH_KEC_TARGET=1,SILPH_KEC_MOVED=2,SILPH_KEC_AUTHORIZE=3,SILPH_KEC_ACK=4,SILPH_KEC_EMPTY=5,SILPH_KEC_WAIT=6};
typedef struct {
    SilphScheduleIdentity identity;
    uint64_t sequence,request;
    uint32_t kind,move,accepted;
    double latitude,longitude;
    char source[97];
} SilphKecleonCommand;
typedef struct {
    SilphScheduleIdentity identity;
    uint64_t sequence,request;
    uint32_t phase,move,reason,verified;
    double latitude,longitude;
    char source[97],fort[161],incident[161];
} SilphKecleonEvent;
int silph_kec_command_encode(const SilphKecleonCommand *,void *,size_t);
int silph_kec_command_decode(const void *,size_t,SilphKecleonCommand *);
int silph_kec_event_encode(const SilphKecleonEvent *,void *,size_t);
int silph_kec_event_decode(const void *,size_t,SilphKecleonEvent *);
#endif
