#ifndef SILPH_RECOVERY_POLICY_H
#define SILPH_RECOVERY_POLICY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint64_t since,last;uint32_t empty;} SilphRecovery;
void silph_recovery_pause(SilphRecovery *);
bool silph_recovery_observe(SilphRecovery *,uint64_t,uint32_t,bool);
#endif
