#include "buddy-actions.h"
#include "game-read.h"
#include <stdlib.h>
#include <string.h>
typedef struct {void *read,*managed,*game,*rpc;SilphManagedRef service;SilphScheduleIdentity identity;SilphBuddyMetrics metrics;uint32_t opened;} Buddy;
size_t silph_buddy_size(void){return sizeof(Buddy);}
int silph_buddy_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity){
    if(!memory||bytes!=sizeof(Buddy)||!read||!managed||!game||!identity)return SILPH_MANAGED_ARGUMENT;Buddy *s=memory;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->opened=1;
    int error=silph_game_read_service(game,1,&s->service);if(!error){s->rpc=calloc(1,silph_rpc_action_size());if(!s->rpc)error=SILPH_MANAGED_CAPACITY;}
    if(!error)error=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),read,managed,game,identity);if(error){silph_buddy_close(s);return error;}s->metrics.state=SILPH_BUDDY_CHECK;return 0;
}
int silph_buddy_tick(void *memory,const SilphSchedulePermit *permit,uint64_t now,SilphBuddyMetrics *out){
    Buddy *s=memory;if(!s||!s->opened||!out||!now||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;int error=0;uint32_t phase=s->metrics.state;
    if(phase==SILPH_BUDDY_CHECK){SilphReadValue has={0};error=silph_read_call(s->read,s->service,"get_HasBuddy",SILPH_READ_BOOL,NULL,0,&has);if(!error&&!has.bits){s->metrics.state=SILPH_BUDDY_NONE;*out=s->metrics;return 0;}}
    if(!error&&(phase==SILPH_BUDDY_CHECK||phase==SILPH_BUDDY_REMOVE)){
        SilphRequestInput input={.abi=1};uint32_t operation=phase==SILPH_BUDDY_CHECK?SILPH_RPC_BUDDY_CHECK:SILPH_RPC_BUDDY_REMOVE;
        s->metrics.state=phase==SILPH_BUDDY_CHECK?SILPH_BUDDY_CHECKING:SILPH_BUDDY_REMOVING;
        error=silph_rpc_action_send(s->rpc,permit,operation,&input,UINT64_C(0x6275646479)+s->metrics.requests,now);s->metrics.requests++;
    }else if(!error&&(phase==SILPH_BUDDY_CHECKING||phase==SILPH_BUDDY_REMOVING)){
        SilphPromiseMetrics pending={0};error=silph_rpc_action_poll(s->rpc,&s->identity,now,&pending);
        if(!error&&pending.state==SILPH_PROMISE_WAITING){*out=s->metrics;return 0;}if(!error&&pending.state!=SILPH_PROMISE_RESOLVED)error=SILPH_MANAGED_STATE;
        SilphManagedRef response={0};bool fresh=false;if(!error)error=silph_rpc_action_take(s->rpc,&s->identity,now,&response,&fresh);if(!error&&!fresh)error=SILPH_MANAGED_STALE;
        SilphReadValue result={0};if(!error)error=silph_read_field(s->read,response,"Result",SILPH_READ_ENUM32,&result);
        if(!error){s->metrics.result=(uint32_t)result.bits;s->metrics.state=result.bits==1?(phase==SILPH_BUDDY_CHECKING?SILPH_BUDDY_REMOVE:SILPH_BUDDY_REMOVED):SILPH_BUDDY_FAILED;}
        if(response.serial){int released=silph_managed_release(s->managed,response);if(!error)error=released;}int reset=silph_rpc_action_reset(s->rpc);if(!error)error=reset;
    }
    if(error){s->metrics.error=(uint32_t)error;s->metrics.state=SILPH_BUDDY_FAILED;}*out=s->metrics;return error;
}
int silph_buddy_close(void *memory){Buddy *s=memory;if(!s||!s->opened)return 0;int a=s->rpc?silph_rpc_action_close(s->rpc):0,b=s->service.serial?silph_managed_release(s->managed,s->service):0;if(a||b)return a?a:b;free(s->rpc);memset(s,0,sizeof(*s));return 0;}
