#include "rpc-actions.h"
#include "game-read.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    void *read,*managed,*game,*pending;
    SilphScheduleIdentity identity;
    SilphManagedRef handler,request;
    uint64_t sequence;
    uint32_t opened,operation;
} RpcAction;
static const char *responses[]={"GetPokestopEncounterOutProto","EncounterPokestopEncounterOutProto","UseItemEncounterOutProto","CatchPokemonOutProto","FortSearchOutProto","RecycleItemOutProto","BuddyMapOutProto","SetBuddyPokemonOutProto","OpenGiftOutProto","ClaimEventPassRewardsResponseProto","ClaimEventPassRewardsResponseProto","CatchPokemonOutProto"};
size_t silph_rpc_action_size(void){return sizeof(RpcAction);}
static int drop(RpcAction *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)memset(ref,0,sizeof(*ref));return r;}
int silph_rpc_action_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity){
    if(!memory||bytes!=sizeof(RpcAction)||!read||!managed||!game||!identity)return SILPH_MANAGED_ARGUMENT;
    RpcAction *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);
    s->pending=calloc(1,silph_managed_promise_size());if(!s->pending)return SILPH_MANAGED_CAPACITY;
    s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->opened=1;
    int status=silph_game_read_rpc_handler(game,&s->handler);if(status){silph_rpc_action_close(s);return status;}return 0;
}
int silph_rpc_action_send(void *memory,const SilphSchedulePermit *permit,uint32_t operation,const SilphRequestInput *input,uint64_t ticket,uint64_t now){
    RpcAction *s=memory;if(!s||!s->opened||s->operation||!ticket||!now||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(silph_proto_validate(operation,input)||s->sequence==UINT64_MAX)return SILPH_MANAGED_ARGUMENT;
    int status=silph_rpc_prepare(s->read,operation,s->handler);
    if(!status)status=silph_proto_request(s->read,operation,input,&s->request);
    SilphReadValue promise={0};
    if(!status){s->operation=operation;status=silph_action_rpc(s->read,permit,operation,s->handler,s->request,&promise);}
    if(!status){status=silph_managed_promise_open_rpc(s->pending,silph_managed_promise_size(),s->read,s->managed,promise.reference,&s->identity,
        ++s->sequence,ticket,now,"Holoholo.Rpc",responses[operation-1]);}
    int freed=drop(s,&promise.reference);if(!status)status=freed;
    if(status&&!s->operation)drop(s,&s->request);return status;
}
int silph_rpc_action_poll(void *memory,const SilphScheduleIdentity *identity,uint64_t now,SilphPromiseMetrics *output){
    RpcAction *s=memory;if(!s||!s->opened||!s->operation)return SILPH_MANAGED_STATE;return silph_managed_promise_poll(s->pending,identity,now,output);
}
int silph_rpc_action_take(void *memory,const SilphScheduleIdentity *identity,uint64_t now,SilphManagedRef *output,bool *fresh){
    RpcAction *s=memory;if(!s||!s->opened||!s->operation)return SILPH_MANAGED_STATE;return silph_managed_promise_take(s->pending,identity,now,output,fresh);
}
int silph_rpc_action_reset(void *memory){
    RpcAction *s=memory;if(!s||!s->opened)return SILPH_MANAGED_STATE;
    int a=silph_managed_promise_close(s->pending),b=drop(s,&s->request);if(!a&&!b)s->operation=0;return a?a:b;
}
int silph_rpc_action_close(void *memory){
    RpcAction *s=memory;if(!s||!s->opened)return 0;
    int a=silph_rpc_action_reset(s),b=drop(s,&s->handler);if(a||b)return a?a:b;free(s->pending);memset(s,0,sizeof(*s));return 0;
}
