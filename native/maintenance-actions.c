#include "maintenance-actions.h"
#include <stdlib.h>
#include <string.h>
#ifdef SILPH_PILOT_SERVICE
#include "response-rewards.h"
#endif
typedef struct {void *read,*managed,*game,*rpc;SilphScheduleIdentity identity;SilphMaintenanceMetrics metrics;uint64_t sync_started;uint32_t opened,operation;} Maintenance;
uint32_t silph_discard_count(uint32_t before,uint32_t floor,uint32_t quota){if(before>100000||floor>100000||!quota||quota>50||before<=floor)return 0;uint32_t n=before-floor;return n<quota?n:quota;}
size_t silph_maintenance_size(void){return sizeof(Maintenance);}
int silph_maintenance_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity){
    if(!memory||bytes!=sizeof(Maintenance)||!read||!managed||!game||!identity)return SILPH_MANAGED_ARGUMENT;
    Maintenance *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->rpc=calloc(1,silph_rpc_action_size());if(!s->rpc)return SILPH_MANAGED_CAPACITY;
    s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->opened=1;
    int status=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),read,managed,game,identity);if(status)silph_maintenance_close(s);return status;
}
int silph_maintenance_spin_filtered(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,SilphStopPredicate accept,void *context){
    Maintenance *s=memory;if(!s||!s->opened||s->metrics.state||!snapshot||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(snapshot->item_count>=snapshot->max_items){s->metrics.state=3;s->metrics.result=4;return 0;}
    SilphGameStop stop;int status=silph_game_read_stop(s->game,80,0,accept,context,&stop);if(status)return status;
    SilphRequestInput input={.abi=1,.latitude=stop.latitude,.longitude=stop.longitude,.player_latitude=stop.player_latitude,.player_longitude=stop.player_longitude};
    memcpy(input.fort,stop.fort,sizeof(input.fort));memcpy(s->metrics.fort,stop.fort,sizeof(s->metrics.fort));
    s->operation=SILPH_RPC_SPIN;status=silph_rpc_action_send(s->rpc,permit,SILPH_RPC_SPIN,&input,1,now);
    if(!status){s->metrics.state=1;s->metrics.requests++;}else s->metrics.error=(uint32_t)status;return status;
}
int silph_maintenance_spin(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now){
    return silph_maintenance_spin_filtered(memory,permit,snapshot,now,NULL,NULL);
}
int silph_maintenance_discard(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,int32_t item,uint32_t floor,uint32_t quota,uint64_t now){
    Maintenance *s=memory;if(!s||!s->opened||s->metrics.state||!snapshot||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(item<=0||item==4||floor>100000||!quota||quota>50)return SILPH_MANAGED_ARGUMENT;
    uint32_t before=0;int status=silph_game_read_item(s->game,item,&before);if(status)return status;
    s->metrics.item=(uint32_t)item;s->metrics.floor=floor;s->metrics.before=before;s->metrics.requested=silph_discard_count(before,floor,quota);
    if(!s->metrics.requested){s->metrics.state=3;return 0;}
    SilphRequestInput input={.abi=1,.item=item,.count=(int32_t)s->metrics.requested};s->operation=SILPH_RPC_DISCARD;
    status=silph_rpc_action_send(s->rpc,permit,SILPH_RPC_DISCARD,&input,(uint32_t)item,now);
    if(!status){s->metrics.state=1;s->metrics.requests++;}else s->metrics.error=(uint32_t)status;return status;
}
int silph_maintenance_poll(void *memory,const SilphScheduleIdentity *identity,uint64_t now,SilphMaintenanceMetrics *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));Maintenance *s=memory;if(!s||!s->opened)return SILPH_MANAGED_STATE;
    int status=0;
    if(s->metrics.state==1){
        SilphPromiseMetrics metrics;status=silph_rpc_action_poll(s->rpc,identity,now,&metrics);
        if(!status&&metrics.state==SILPH_PROMISE_RESOLVED){
            SilphManagedRef result={0};bool fresh=false;SilphReadValue value={0};
            status=silph_rpc_action_take(s->rpc,identity,now,&result,&fresh);
            if(!status)status=silph_read_field(s->read,result,"Result",SILPH_READ_ENUM32,&value);
            if(!status){s->metrics.result=(uint32_t)value.bits;s->metrics.state=3;
                if(value.bits==1&&s->operation==SILPH_RPC_SPIN){
                    status=silph_read_field(s->read,result,"XpAwarded",SILPH_READ_I32,&value);
                    if(!status&&value.bits>100000)status=SILPH_MANAGED_CONTRACT;
                    if(!status){s->metrics.spins++;s->metrics.xp=value.bits;}
#ifdef SILPH_PILOT_SERVICE
                    if(!status)s->metrics.reward_error=(uint32_t)silph_spin_rewards(s->read,s->managed,result,&s->metrics.xp,&s->metrics.stardust);
#endif
                }
                if(!status&&s->operation==SILPH_RPC_SPIN){status=silph_read_field(s->read,result,"CooldownComplete",SILPH_READ_I64,&value);if(!status)s->metrics.cooldown_ms=value.bits;}
                else if(!status&&s->metrics.result==1){
                    status=silph_read_field(s->read,result,"NewCount",SILPH_READ_I32,&value);
                    if(!status&&(value.bits>100000||value.bits<s->metrics.floor))status=SILPH_MANAGED_CONTRACT;
                    if(!status){s->metrics.after=(uint32_t)value.bits;s->metrics.state=2;s->sync_started=now;}
                }
            }
            int a=result.serial?silph_managed_release(s->managed,result):0,b=silph_rpc_action_reset(s->rpc);if(!status)status=a?a:b;
        }else if(!status&&metrics.state!=SILPH_PROMISE_WAITING)s->metrics.state=4;
    }else if(s->metrics.state==2){
        uint32_t count=0;status=silph_game_read_item(s->game,(int32_t)s->metrics.item,&count);
        if(!status&&count==s->metrics.after){s->metrics.discarded=s->metrics.requested;s->metrics.state=3;}
        else if(!status&&now-s->sync_started>=UINT64_C(5000000000))s->metrics.state=4;
    }
    if(status)s->metrics.error=(uint32_t)status;*output=s->metrics;return status;
}
int silph_maintenance_close(void *memory){Maintenance *s=memory;if(!s||!s->opened)return 0;int status=silph_rpc_action_close(s->rpc);if(status)return status;free(s->rpc);memset(s,0,sizeof(*s));return 0;}
