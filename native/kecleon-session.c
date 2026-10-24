#include "kecleon-session.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
    void *read,*managed,*game,*link,*plan,*action,*maintenance;
    SilphScheduleIdentity identity;SilphSessionConfig config;SilphKecleonSessionMetrics metrics;SilphKecleonEvent event;
    uint64_t event_sequence,clean_due,inventory_after;uint32_t opened,action_opened,maintenance_opened,clean_index,clean_batches,event_dirty;
} Session;
size_t silph_kecleon_session_size(void){return sizeof(Session);}
int silph_kecleon_session_open(void *memory,size_t bytes,void *read,void *managed,void *game,void *link,const SilphScheduleIdentity *id,const SilphSessionConfig *config,uint64_t now){
    if(!memory||bytes!=sizeof(Session)||!read||!managed||!game||!link||!id||!config||config->mode!=SILPH_MODE_KECLEON||(config->flags&SILPH_CFG_READONLY))return SILPH_MANAGED_ARGUMENT;
    Session *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->link=link;s->identity=*id;s->config=*config;
    s->plan=calloc(1,silph_kecleon_plan_size());s->action=calloc(1,silph_kecleon_actions_size());s->maintenance=calloc(1,silph_maintenance_size());
    if(!s->plan||!s->action||!s->maintenance){free(s->plan);free(s->action);free(s->maintenance);memset(s,0,bytes);return SILPH_MANAGED_CAPACITY;}
    int status=silph_kecleon_plan_open(s->plan,silph_kecleon_plan_size(),id,config->max_encounters,now);
    if(status){free(s->plan);free(s->action);free(s->maintenance);memset(s,0,bytes);return status;}s->opened=1;return 0;
}
bool silph_kecleon_session_inventory_due(void *memory){
    Session *s=memory;if(!s||!s->opened)return true;if(s->maintenance_opened)return false;if(s->action_opened)return silph_kecleon_inventory_due(s->action);
    struct timespec t={0};if(clock_gettime(CLOCK_MONOTONIC,&t))return false;uint64_t now=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
    if(now<s->inventory_after)return false;s->inventory_after=now+UINT64_C(2000000000);return true;
}
static int command(Session *s,const SilphKecleonCommand *c,uint64_t now){
    if(memcmp(&c->identity,&s->identity,sizeof(s->identity)))return SILPH_MANAGED_STALE;
    SilphKecleonPlanState p;int status=silph_kecleon_plan_state(s->plan,&p);if(status)return status;
    if(c->request!=p.request)return 0;
    if(c->kind==SILPH_KEC_TARGET&&p.phase==SILPH_KPLAN_NEED_TARGET){SilphKecleonCandidate target={.request=c->request,.latitude=c->latitude,.longitude=c->longitude};memcpy(target.source_id,c->source,97);return silph_kecleon_plan_target(s->plan,&s->identity,&target,now);}
    if(c->kind==SILPH_KEC_MOVED&&(p.phase==SILPH_KPLAN_NAVIGATE||p.phase==SILPH_KPLAN_NUDGE)&&c->move==p.move_id)return silph_kecleon_plan_moved(s->plan,&s->identity,c->request,c->move,!!c->accepted,now);
    if(c->kind==SILPH_KEC_AUTHORIZE&&p.phase==SILPH_KPLAN_AUTHORIZE)return silph_kecleon_plan_authorize(s->plan,&s->identity,c->request,!!c->accepted,now);
    if(c->kind==SILPH_KEC_ACK&&p.phase==SILPH_KPLAN_RESULT){
        if(s->action_opened){status=silph_kecleon_actions_close(s->action);if(status)return status;s->action_opened=0;}
        memset(&s->metrics.current,0,sizeof(s->metrics.current));return silph_kecleon_plan_acknowledge(s->plan,&s->identity,c->request,now);
    }
    if(c->kind==SILPH_KEC_EMPTY&&p.phase==SILPH_KPLAN_NEED_TARGET)return silph_kecleon_plan_empty(s->plan,&s->identity,c->request,now);
    return 0;
}
static int clean(Session *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool *busy){
    *busy=false;int status=0;
    if(!s->maintenance_opened){
        if(!(s->config.flags&SILPH_CFG_CLEAN)||s->action_opened||now<s->clean_due)return 0;
        if(s->clean_index>=s->config.limit_count||s->clean_batches>=4){s->clean_index=0;s->clean_batches=0;s->clean_due=now+UINT64_C(15000000000);return 0;}
        status=silph_maintenance_open(s->maintenance,silph_maintenance_size(),s->read,s->managed,s->game,&s->identity);
        if(!status){s->maintenance_opened=1;s->clean_batches++;status=silph_maintenance_discard(s->maintenance,permit,snapshot,(int32_t)s->config.limits[s->clean_index].item,s->config.limits[s->clean_index].floor,50,now);}
    }
    if(!status){SilphMaintenanceMetrics m={0};status=silph_maintenance_poll(s->maintenance,&s->identity,now,&m);
        if(!status&&(m.state==3||m.state==4)){
            s->metrics.deleted+=m.discarded;if(!m.discarded||m.after<=m.floor)s->clean_index++;
            if(m.state==4)return SILPH_MANAGED_STALE;
            status=silph_maintenance_close(s->maintenance);if(!status)s->maintenance_opened=0;
        }
    }
    *busy=!!s->maintenance_opened;return status;
}
static int update_event(Session *s){
    SilphKecleonPlanState *p=&s->metrics.plan;SilphKecleonEvent e={.identity=s->identity,.request=p->request,.phase=p->phase,.move=p->move_id,.reason=p->reason,.verified=p->verified,
        .latitude=p->candidate.latitude,.longitude=p->candidate.longitude};
    if(p->phase!=SILPH_KPLAN_NEED_TARGET&&p->phase!=SILPH_KPLAN_FINISHED){memcpy(e.source,p->candidate.source_id,97);memcpy(e.fort,p->fort,161);memcpy(e.incident,p->incident,161);}
    const SilphKecleonEvent *before=&s->event;
    if(!before->sequence||before->request!=e.request||before->phase!=e.phase||before->move!=e.move||before->reason!=e.reason||before->verified!=e.verified
        ||before->latitude!=e.latitude||before->longitude!=e.longitude||strcmp(before->source,e.source)||strcmp(before->fort,e.fort)||strcmp(before->incident,e.incident)){
        e.sequence=++s->event_sequence;s->event=e;s->event_dirty=1;
    }return 0;
}
int silph_kecleon_session_publish(void *memory){Session *s=memory;if(!s||!s->opened)return SILPH_MANAGED_STATE;if(!s->event_dirty)return 0;
    int status=silph_session_link_event(s->link,&s->event);if(status==SILPH_SCHEDULE_BUSY)return 0;if(!status)s->event_dirty=0;return status;
}
int silph_kecleon_session_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,uint64_t wall,SilphKecleonSessionMetrics *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));Session *s=memory;
    if(!s||!s->opened||!snapshot||!now||!wall||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    int status=0;
    for(unsigned n=0;n<8;n++){SilphKecleonCommand c;int popped=silph_session_link_pop(s->link,&c);if(popped==SILPH_SCHEDULE_BUSY||popped==SILPH_SCHEDULE_STATE)break;if(popped){status=popped;break;}status=command(s,&c,now);if(status)break;}
    if(!status&&!s->action_opened)status=silph_kecleon_plan_advance(s->plan,&s->identity,now);
    if(!status)status=silph_kecleon_plan_state(s->plan,&s->metrics.plan);
    bool busy=false;if(!status&&s->metrics.plan.phase!=SILPH_KPLAN_FINISHED)status=clean(s,permit,snapshot,now,&busy);
    SilphKecleonPlanState *p=&s->metrics.plan;
    if(!status&&!busy&&p->phase==SILPH_KPLAN_SCAN&&now>=p->scan_after_ns){SilphGameStop stop={0};int scanned=silph_game_read_stop_at(s->game,50,p->candidate.latitude,p->candidate.longitude,3,&stop);
        if(scanned&&scanned!=SILPH_MANAGED_NOT_READY)status=scanned;
        else status=silph_kecleon_plan_scan(s->plan,&s->identity,p->request,scanned?NULL:&stop,now,wall);
    }
    if(!status&&!busy&&p->phase==SILPH_KPLAN_RUN){
        if(!s->action_opened){SilphKecleonOptions options={.abi=1,.filters=s->config.filters,.quality=s->config.quality,.curve=!!(s->config.flags&SILPH_CFG_CURVE),.use_berries=!!(s->config.flags&SILPH_CFG_BERRIES),.berry_count=s->config.berry_count};
            for(unsigned i=0;i<5;i++)options.berries[i]=(int32_t)s->config.berries[i];
            status=silph_kecleon_actions_open(s->action,silph_kecleon_actions_size(),s->read,s->managed,s->game,&s->identity,&options);
            if(!status){s->action_opened=1;status=silph_kecleon_bind_candidate(s->action,&p->candidate);}
            if(!status)status=silph_kecleon_bind_stop(s->action,p->fort,p->incident);
        }
        if(!status)status=silph_kecleon_actions_tick(s->action,permit,snapshot,now,wall,&s->metrics.current);
        if(!status&&s->metrics.current.uncertain)status=SILPH_MANAGED_STATE;
        if(!status&&(s->metrics.current.state==SILPH_KECLEON_DONE||s->metrics.current.state==SILPH_KECLEON_UNVERIFIED)){
            status=silph_kecleon_actions_close(s->action);if(!status){s->action_opened=0;status=silph_kecleon_plan_result(s->plan,&s->identity,p->request,&s->metrics.current,now);}
        }
    }
    if(!status)status=silph_kecleon_plan_state(s->plan,&s->metrics.plan);if(!status)status=update_event(s);
    if(status)s->metrics.error=(uint32_t)status;*out=s->metrics;return status;
}
int silph_kecleon_session_close(void *memory){Session *s=memory;if(!s||!s->opened)return 0;
    int a=s->action_opened?silph_kecleon_actions_close(s->action):0,b=s->maintenance_opened?silph_maintenance_close(s->maintenance):0;if(a||b)return a?a:b;
    int status=silph_kecleon_plan_close(s->plan);if(status)return status;free(s->plan);free(s->action);free(s->maintenance);memset(s,0,sizeof(*s));return 0;
}
