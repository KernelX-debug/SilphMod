#ifndef SILPH_RPC_ACTIONS_H
#define SILPH_RPC_ACTIONS_H
#include "managed-proto.h"
#include "managed-promise.h"
size_t silph_rpc_action_size(void);
int silph_rpc_action_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *);
int silph_rpc_action_send(void *,const SilphSchedulePermit *,uint32_t,const SilphRequestInput *,uint64_t,uint64_t);
int silph_rpc_action_poll(void *,const SilphScheduleIdentity *,uint64_t,SilphPromiseMetrics *);
int silph_rpc_action_take(void *,const SilphScheduleIdentity *,uint64_t,SilphManagedRef *,bool *);
int silph_rpc_action_reset(void *);
int silph_rpc_action_close(void *);
#endif
