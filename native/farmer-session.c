#define _POSIX_C_SOURCE 200809L
#include "farmer-session.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    void *read,*managed,*game,*farmer,*maintenance;
#ifdef SILPH_PRESENT_SCOPED
    void *buddy;
#endif
#ifdef SILPH_PILOT_SERVICE
    void *research,*go_pass;SilphManagedRef pass_reward;
#endif
    SilphScheduleIdentity identity;
    SilphFarmerSessionOptions options;
    SilphFarmerSessionMetrics metrics;
    struct {char fort[161];uint64_t until;} stops[256];
    uint64_t clock,spin_due,clean_due;
    uint32_t opened,farmer_opened,maintenance_opened,lane,clean_index,clean_round;
} Session;
static uint64_t clock_ns(void){struct timespec t={0};return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
int silph_farmer_session_validate(const SilphFarmerSessionOptions *options){
    if(!options||options->abi!=1||options->catches>1||options->spins>1||options->clean>1||options->research>1||(!options->catches&&!options->spins&&!options->clean)
        ||options->max_spins>50000||options->limit_count>64||(options->clean&&!options->limit_count))return SILPH_MANAGED_ARGUMENT;
    for(unsigned i=0;i<options->limit_count;i++){
        if(options->limits[i].item<=0||options->limits[i].item==4||options->limits[i].floor>100000)return SILPH_MANAGED_ARGUMENT;
        for(unsigned j=0;j<i;j++)if(options->limits[j].item==options->limits[i].item)return SILPH_MANAGED_ARGUMENT;
    }
    return 0;
}
size_t silph_farmer_session_size(void){return sizeof(Session);}
int silph_farmer_session_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,const SilphFarmerSessionOptions *options,uint32_t transfers){
    if(!memory||bytes!=sizeof(Session)||!read||!managed||!game||!identity||(!identity->account_hi&&!identity->account_lo)||transfers>1)return SILPH_MANAGED_ARGUMENT;
    if(silph_farmer_session_validate(options))return SILPH_MANAGED_ARGUMENT;
    Session *s=memory;if(s->opened)return SILPH_MANAGED_STATE;
    memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->options=*options;s->opened=1;s->metrics.state=SILPH_SESSION_RUNNING;
    int status=0;
    if(options->catches){
        s->farmer=calloc(1,silph_farmer_actions_size());if(!s->farmer)status=SILPH_MANAGED_CAPACITY;
        if(!status)status=silph_farmer_actions_open(s->farmer,silph_farmer_actions_size(),read,managed,game,identity,transfers);
        if(!status){s->farmer_opened=1;SilphFarmerOptions config=options->farmer;config.continuous=1;status=silph_farmer_actions_configure(s->farmer,&config);}
    }
    if(!status&&(options->spins||options->clean)){s->maintenance=calloc(1,silph_maintenance_size());if(!s->maintenance)status=SILPH_MANAGED_CAPACITY;}
#ifdef SILPH_PILOT_SERVICE
    if(!status&&options->research){s->research=calloc(1,silph_research_size());if(!s->research)status=SILPH_MANAGED_CAPACITY;
        if(!status)status=silph_research_open(s->research,silph_research_size(),read,managed,game,identity,transfers,options->farmer.use_berries);
        if(!status){s->go_pass=calloc(1,silph_pass_size());if(!s->go_pass)status=SILPH_MANAGED_CAPACITY;else status=silph_pass_open(s->go_pass,silph_pass_size(),read,managed,game,identity);}}
#endif
    #ifdef SILPH_PRESENT_SCOPED
    if(!status&&options->buddy.flags){s->buddy=calloc(1,silph_buddy_care_size());if(!s->buddy)status=SILPH_MANAGED_CAPACITY;
        if(!status)status=silph_buddy_care_open(s->buddy,silph_buddy_care_size(),read,managed,game,identity,&options->buddy);}
#endif
    if(status)silph_farmer_session_close(s);return status;
}
bool silph_farmer_session_inventory_due(void *memory){
    Session *s=memory;return !s||!s->opened||(!s->lane&&(!s->farmer_opened||silph_farmer_actions_inventory_due(s->farmer)));
}
static bool stop_allowed(void *memory,const char *fort){
    Session *s=memory;for(unsigned i=0;i<256;i++)if(!strcmp(s->stops[i].fort,fort)&&s->stops[i].until>s->clock)return false;return true;
}
static void remember_stop(Session *s,const SilphMaintenanceMetrics *m){
    if(!m->fort[0])return;unsigned chosen=0;
    for(unsigned i=0;i<256;i++){
        if(!strcmp(s->stops[i].fort,m->fort)||!s->stops[i].fort[0]){chosen=i;break;}
        if(s->stops[i].until<s->stops[chosen].until)chosen=i;
    }
    memcpy(s->stops[chosen].fort,m->fort,sizeof(s->stops[chosen].fort));
    s->stops[chosen].until=s->clock+(m->result==1||m->result==3?UINT64_C(300000000000):UINT64_C(5000000000));
}
static uint32_t item_floor(Session *s,const SilphGameSnapshot *snapshot,unsigned index){
    uint32_t floor=s->options.limits[index].floor;
#ifdef SILPH_PRESENT_SCOPED
    uint32_t buddy=silph_buddy_care_floor(s->buddy,snapshot,(uint32_t)s->options.limits[index].item);if(buddy>floor)floor=buddy;
#else
    (void)snapshot;
#endif
    return floor;
}
static int maintenance_open(Session *s){
    int status=silph_maintenance_open(s->maintenance,silph_maintenance_size(),s->read,s->managed,s->game,&s->identity);
    if(!status)s->maintenance_opened=1;return status;
}
static int maintenance_close(Session *s){
    int status=silph_maintenance_close(s->maintenance);if(!status){s->maintenance_opened=0;s->lane=0;}return status;
}
static int poll(Session *s,uint64_t now){
    SilphMaintenanceMetrics m={0};int status=silph_maintenance_poll(s->maintenance,&s->identity,now,&m);
    s->metrics.maintenance_state=m.state;
    if(!status&&(m.state==3||m.state==4)){
        if(s->lane==1){s->metrics.spin_requests+=m.requests;s->metrics.spins+=m.spins;s->metrics.spin_xp+=m.xp;s->metrics.spin_stardust+=m.stardust;if(m.reward_error)s->metrics.reward_error=m.reward_error;s->metrics.spin_result=m.result;remember_stop(s,&m);s->spin_due=now+UINT64_C(2000000000);
#ifdef SILPH_PILOT_SERVICE
            if(s->research&&m.spins)silph_research_refresh(s->research,now);
#endif
        }
        else{s->metrics.delete_requests+=m.requests;s->metrics.deleted+=m.discarded;if(!m.discarded||m.after<=m.floor)s->clean_index++;}
        if(m.state==4){s->metrics.state=SILPH_SESSION_UNVERIFIED;s->metrics.reason=SILPH_SESSION_RESPONSE_UNKNOWN;}
        status=maintenance_close(s);
    }
    return status;
}
#ifdef SILPH_PILOT_SERVICE
static int research_tick(Session *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool *accepted){
    uint64_t started=clock_ns();SilphManagedRef target={0};uint64_t encounter=0;uint32_t species=0;
    bool capture=s->farmer_opened&&s->metrics.farmer.state==SILPH_FARMER_SELECT&&!s->lane;*accepted=false;
    int status=silph_research_tick(s->research,permit,snapshot,now,capture,&target,&encounter,&species);
    if(!status&&target.serial){uint32_t source=silph_research_target_source(s->research,encounter);status=silph_farmer_research_target(s->farmer,target,encounter,species,source);if(!status){*accepted=true;status=silph_farmer_actions_metrics(s->farmer,&s->metrics.farmer);}}
    if(target.serial){if(status)silph_research_destroy(s->read,target);int freed=silph_managed_release(s->managed,target);if(!status)status=freed;}
    if(!status)status=silph_research_metrics(s->research,&s->metrics.research);
    uint64_t spent=clock_ns()-started;s->metrics.research_ns+=spent;if(spent>s->metrics.max_research_ns)s->metrics.max_research_ns=spent;return status;
}
#endif
int silph_farmer_session_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,SilphFarmerSessionMetrics *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));Session *s=memory;
    if(!s||!s->opened||!snapshot||!now||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(s->identity.account_hi!=snapshot->account_hi||s->identity.account_lo!=snapshot->account_lo)return SILPH_MANAGED_STALE;
    if(now<s->clock)return SILPH_MANAGED_STATE;s->clock=now;s->metrics.callbacks++;int status=0;uint64_t began=clock_ns();bool maintaining=false;
    if(s->metrics.state!=SILPH_SESSION_RUNNING){*output=s->metrics;return 0;}
#ifdef SILPH_PILOT_SERVICE
    bool research_stepped=false;
    if(s->go_pass){
        bool capture=s->farmer_opened&&snapshot->pokemon_count<snapshot->max_pokemon&&(snapshot->items[0]||snapshot->items[1]||snapshot->items[2]);SilphManagedRef reward={0};
        status=silph_pass_tick(s->go_pass,permit,snapshot,now,capture,!s->pass_reward.serial,&reward,&s->metrics.go_pass);
        if(!status&&reward.serial){s->pass_reward=reward;reward=(SilphManagedRef){0};}
        if(!status&&s->pass_reward.serial&&capture&&s->metrics.farmer.state==SILPH_FARMER_SELECT&&!s->lane&&silph_research_available(s->research)){
            int supplied=silph_research_supply(s->research,permit,snapshot,s->pass_reward,now);if(supplied&&supplied!=SILPH_MANAGED_NOT_READY)s->metrics.research.error=(uint32_t)supplied;
            int freed=silph_managed_release(s->managed,s->pass_reward);if(!status)status=freed;s->pass_reward=(SilphManagedRef){0};
        }
    }
    if(!status&&s->research&&silph_research_pending(s->research)){research_stepped=true;status=research_tick(s,permit,snapshot,now,&maintaining);}
#endif
    #ifdef SILPH_PRESENT_SCOPED
    if(!status&&!maintaining&&s->buddy){bool busy=false;status=silph_buddy_care_tick(s->buddy,permit,snapshot,now,silph_farmer_session_idle(s),&busy);maintaining=busy;}
#endif
    if(!status&&!maintaining&&s->lane){maintaining=true;status=poll(s,now);}
    else if(!status&&!maintaining){
        bool boundary=!s->farmer_opened||s->metrics.farmer.state==0||s->metrics.farmer.state==SILPH_FARMER_SELECT,force_farmer=false;
        if(boundary&&s->options.clean&&now>=s->clean_due){
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
            for(unsigned skipped=0;s->clean_index<s->options.limit_count&&skipped<4;skipped++){
                uint32_t count=0;status=silph_game_read_item(s->game,s->options.limits[s->clean_index].item,&count);
                if(status||count>item_floor(s,snapshot,s->clean_index))break;s->clean_index++;
                if(clock_ns()-began>=UINT64_C(500000))break;
            }
            if(status){s->metrics.error=(uint32_t)status;*output=s->metrics;return status;}
#endif
            maintaining=true;
            if(s->clean_index>=s->options.limit_count){s->clean_index=0;s->clean_due=now+UINT64_C(15000000000);s->clean_round=0;}
            else if(s->clean_round>=4&&s->farmer_opened){s->clean_round=0;maintaining=false;force_farmer=true;}
            else{
                status=maintenance_open(s);s->lane=2;s->clean_round++;
                if(!status)status=silph_maintenance_discard(s->maintenance,permit,snapshot,s->options.limits[s->clean_index].item,item_floor(s,snapshot,s->clean_index),50,now);
                if(!status)status=poll(s,now);
            }
        }
        if(!status&&!maintaining&&!force_farmer&&boundary&&s->options.spins&&now>=s->spin_due){
            maintaining=true;status=maintenance_open(s);s->lane=1;s->spin_due=now+UINT64_C(2000000000);
            if(!status)status=silph_maintenance_spin_filtered(s->maintenance,permit,snapshot,now,stop_allowed,s);
            if(status==SILPH_MANAGED_NOT_READY){status=maintenance_close(s);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
                maintaining=false;
#endif
            }
            else if(!status)status=poll(s,now);
        }
#ifdef SILPH_PILOT_SERVICE
        if(!status&&!maintaining&&!research_stepped&&boundary&&s->research&&(silph_research_pending(s->research)||silph_research_due(s->research,now))){
            status=research_tick(s,permit,snapshot,now,&maintaining);
        }
#endif
        if(!status&&!maintaining&&s->farmer_opened){
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
            SilphGameSnapshot current=*snapshot;
            for(unsigned step=0;step<3;step++){
                uint32_t before=s->metrics.farmer.state;
                status=silph_farmer_actions_tick(s->farmer,permit,&current,now,&s->metrics.farmer);
                uint32_t after=s->metrics.farmer.state;
                if(status||before==after||clock_ns()-began>=UINT64_C(750000)||!silph_schedule_permitted(permit,true))break;
                bool resolved=before==SILPH_FARMER_ENCOUNTER||before==SILPH_FARMER_CAPTURE||before==SILPH_FARMER_TRANSFER||before==SILPH_FARMER_BERRY;
                bool followup=resolved&&(after==SILPH_FARMER_RETRY||after==SILPH_FARMER_OWNED||after==SILPH_FARMER_SYNC);
                if(after==SILPH_FARMER_SELECT){
                    bool pending=(s->options.clean&&now>=s->clean_due)||(s->options.spins&&now>=s->spin_due)
                        ||(s->research&&(silph_research_pending(s->research)||silph_research_due(s->research,now)));
                    followup=!pending&&(resolved||before==SILPH_FARMER_SYNC||before==SILPH_FARMER_OWNED);
                }
                if(!followup||step==2)break;
                if(silph_farmer_actions_inventory_due(s->farmer)){
                    status=silph_game_read_refresh(s->game,true,&current);if(!status)s->metrics.refreshes++;
                    if(!status&&(current.account_hi!=s->identity.account_hi||current.account_lo!=s->identity.account_lo))status=SILPH_MANAGED_STALE;
                }
                if(status||clock_ns()-began>=UINT64_C(1500000)||!silph_schedule_permitted(permit,true))break;
                s->metrics.followups++;
            }
#else
            uint32_t before=s->metrics.farmer.state;
            status=silph_farmer_actions_tick(s->farmer,permit,snapshot,now,&s->metrics.farmer);
            uint32_t after=s->metrics.farmer.state;uint64_t spent=clock_ns()-began;
            bool resolved=before==SILPH_FARMER_ENCOUNTER||before==SILPH_FARMER_CAPTURE||before==SILPH_FARMER_TRANSFER||before==SILPH_FARMER_BERRY;
            bool followup=after==SILPH_FARMER_RETRY||after==SILPH_FARMER_OWNED||after==SILPH_FARMER_SYNC;
            if(!status&&before!=after&&resolved&&followup&&spent<UINT64_C(500000)&&silph_schedule_permitted(permit,true)){
                SilphGameSnapshot fresh={0};status=silph_game_read_refresh(s->game,silph_farmer_actions_inventory_due(s->farmer),&fresh);
                if(!status)s->metrics.refreshes++;
                if(!status&&(fresh.account_hi!=s->identity.account_hi||fresh.account_lo!=s->identity.account_lo))status=SILPH_MANAGED_STALE;
                if(!status&&clock_ns()-began<UINT64_C(1000000)&&silph_schedule_permitted(permit,true)){
                    s->metrics.followups++;status=silph_farmer_actions_tick(s->farmer,permit,&fresh,now,&s->metrics.farmer);
                }
            }
#endif
        }
    }
    if(!status&&s->metrics.state==SILPH_SESSION_RUNNING){
        if(s->farmer_opened&&s->metrics.farmer.state==SILPH_FARMER_DONE){s->metrics.state=SILPH_SESSION_DONE;s->metrics.reason=SILPH_SESSION_CATCH_LIMIT;}
        else if(s->options.max_spins&&s->metrics.spins>=s->options.max_spins){s->metrics.state=SILPH_SESSION_DONE;s->metrics.reason=SILPH_SESSION_SPIN_LIMIT;}
    }
    uint64_t ended=clock_ns(),elapsed=ended>=began?ended-began:0;
    if(maintaining){s->metrics.maintenance_ns+=elapsed;if(elapsed>s->metrics.max_maintenance_ns)s->metrics.max_maintenance_ns=elapsed;}
    else{s->metrics.farmer_ns+=elapsed;if(elapsed>s->metrics.max_farmer_ns)s->metrics.max_farmer_ns=elapsed;}
    if(status)s->metrics.error=(uint32_t)status;*output=s->metrics;return status;
}
bool silph_farmer_session_idle(void *memory){
    Session *s=memory;return s&&s->opened&&s->metrics.state==SILPH_SESSION_RUNNING&&!s->lane&&(!s->farmer_opened||s->metrics.farmer.state==0||s->metrics.farmer.state==SILPH_FARMER_SELECT)
#ifdef SILPH_PILOT_SERVICE
        &&(!s->research||!silph_research_pending(s->research))
#endif
        ;
}
int silph_farmer_session_close(void *memory){
    Session *s=memory;if(!s||!s->opened)return 0;int a=s->farmer_opened?silph_farmer_actions_close(s->farmer):0,b=s->maintenance_opened?maintenance_close(s):0;
#ifdef SILPH_PILOT_SERVICE
    if(s->pass_reward.serial){int freed=silph_managed_release(s->managed,s->pass_reward);if(freed)return freed;s->pass_reward=(SilphManagedRef){0};}
    int d=s->go_pass?silph_pass_close(s->go_pass):0;if(d)return d;free(s->go_pass);s->go_pass=NULL;
    int c=s->research?silph_research_close(s->research):0;if(c)return c;free(s->research);s->research=NULL;
#endif
    #ifdef SILPH_PRESENT_SCOPED
    int care=s->buddy?silph_buddy_care_close(s->buddy):0;if(care)return care;free(s->buddy);s->buddy=NULL;
#endif
    if(a||b)return a?a:b;free(s->farmer);free(s->maintenance);memset(s,0,sizeof(*s));return 0;
}
