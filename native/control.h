#ifndef SILPH_CONTROL_H
#define SILPH_CONTROL_H
#include "scheduler.h"

#define SILPH_CONTROL_ABI 1u
#define SILPH_CONTROL_MAGIC UINT64_C(0x53494c5048435431)
#define SILPH_CONTROL_NAME_CAPACITY 96u
#define SILPH_CONTROL_EXTENDED_ABI 2u
#define SILPH_CONTROL_BODY_CAPACITY 1024u
#define SILPH_CONTROL_SNAPSHOT_CAPACITY 896u

typedef struct {
    uint64_t magic, sequence, key_hi, key_lo, process_ticks;
    uint32_t abi, size, pid, command, foreground, reserved;
} SilphControlRequest;

typedef struct {
    uint64_t magic, sequence, executed, canceled, pulses;
    uint32_t abi, size, pid, status, state, reason, queued, readonly;
} SilphControlReply;

typedef struct {
    uint64_t accepted, rejected, pulses, disconnects, sequence;
    uint32_t connected, authenticated, closed, reserved;
} SilphControlMetrics;

typedef struct {
    void *scheduler;
    SilphScheduleLease lease;
    SilphScheduleIdentity identity;
    uint64_t key_hi, key_lo;
    uint32_t controller_uid, reserved;
    int (*message)(void *,const void *,size_t);
    int (*snapshot)(void *,void *,size_t,size_t *);
    void *extension;
} SilphControlConfig;

enum { SILPH_CONTROL_PULSE = 1, SILPH_CONTROL_STOP = 2, SILPH_CONTROL_STATUS = 3, SILPH_CONTROL_ENGINE = 4 };
enum { SILPH_CONTROL_OK = 0, SILPH_CONTROL_ARGUMENT = 20, SILPH_CONTROL_IO = 21, SILPH_CONTROL_CLOSED = 22, SILPH_CONTROL_THREAD = 23 };

size_t silph_control_size(void);
int silph_control_open(void *, size_t, const SilphControlConfig *);
int silph_control_poll(void *, unsigned);
int silph_control_metrics(void *, SilphControlMetrics *);
int silph_control_close(void *);
int silph_control_name(char *, size_t, uint32_t, uint64_t, uint64_t);
int silph_control_address(void *, const char *, unsigned *);
#endif
