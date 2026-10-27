#ifndef SILPH_MANAGED_ACTION_H
#define SILPH_MANAGED_ACTION_H
#include "managed-read.h"
#include "scheduler.h"
#include "transfer-policy.h"

enum { SILPH_ACTION_ENCOUNTER=1, SILPH_ACTION_BERRY=2, SILPH_ACTION_CAPTURE=3 };
#define SILPH_RPC_COUNT 12u
enum { SILPH_RPC_KECLEON_GET=1, SILPH_RPC_KECLEON_OPEN=2, SILPH_RPC_KECLEON_BERRY=3, SILPH_RPC_KECLEON_CAPTURE=4,
    SILPH_RPC_SPIN=5, SILPH_RPC_DISCARD=6,SILPH_RPC_BUDDY_CHECK=7,SILPH_RPC_BUDDY_REMOVE=8,SILPH_RPC_GIFT_OPEN=9,SILPH_RPC_PASS_SLOT=10,SILPH_RPC_PASS_ALL=11,
    SILPH_RPC_ROCKET_CAPTURE=12 };
SILPH_MANAGED_EXPORT int silph_action_enable(void *,SilphManagedResolver,void *);
SILPH_MANAGED_EXPORT int silph_action_call(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,
    const SilphReadArgument *,uint32_t,SilphReadValue *);
SILPH_MANAGED_EXPORT int silph_action_capture_data(void *,const SilphSchedulePermit *,int32_t,float,bool,bool,
    SilphManagedRef *,SilphManagedRef *);
SILPH_MANAGED_EXPORT int silph_action_release(void *,const SilphSchedulePermit *,void *,uint64_t,
    const SilphTransferInput *,SilphManagedRef,SilphManagedRef,SilphReadValue *);
SILPH_MANAGED_EXPORT int silph_rpc_enable(void *,SilphManagedResolver,void *);
SILPH_MANAGED_EXPORT int silph_rpc_prepare(void *,uint32_t,SilphManagedRef);
SILPH_MANAGED_EXPORT int silph_action_rpc(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,SilphManagedRef,SilphReadValue *);
#endif
