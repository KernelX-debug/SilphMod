#include "event-session.h"
#include "event-policy.h"
#include "response-rewards.h"
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {void *promise;uint64_t id,cell,expires,started;uint32_t index;} Pending;
typedef struct {
    void *read,*managed,*game;SilphManagedRef service,directory;SilphScheduleIdentity identity;
    SilphEventHistory history;SilphEventPriority priority;Pending pending[SILPH_EVENT_PENDING];SilphEventMetrics metrics;
    uint64_t next_scan,next_log,paused_until,rearmed,prioritized;uint32_t cursor,opened;
} Events;
static uint64_t clock_ns(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*UINT64_C(1000000000)+t.tv_nsec;}
size_t silph_events_size(void){return sizeof(Events);}
static int drop(Events *s,SilphManagedRef *r){return silph_reward_drop(s->managed,r);}
static int call(Events *s,SilphManagedRef r,const char *name,uint32_t kind,const SilphReadArgument *a,uint32_t n,SilphReadValue *v){return silph_read_call(s->read,r,name,kind,a,n,v);}
static int value(Events *s,SilphManagedRef r,const char *name,uint32_t kind,uint64_t *v){SilphReadValue out={0};int e=call(s,r,name,kind,NULL,0,&out);if(!e)*v=out.bits;return e;}
static int field(Events *s,SilphManagedRef r,const char *name,SilphManagedRef *out){SilphReadValue v={0};int e=silph_read_field(s->read,r,name,SILPH_READ_REFERENCE,&v);if(!e)*out=v.reference;return e;}
static int item(Events *s,SilphManagedRef list,uint32_t i,SilphManagedRef *out){SilphReadArgument a={.kind=SILPH_READ_I32,.value.bits=i};SilphReadValue v={0};int e=call(s,list,"get_Item",SILPH_READ_REFERENCE,&a,1,&v);if(!e)*out=v.reference;return e;}
static int rewards(Events *s,SilphManagedRef result,bool *repeat,bool *rare){
    *repeat=false;*rare=false;
    SilphManagedRef list={0},loot={0},entries={0},entry={0};uint64_t count=0;SilphEventMetrics m={0};int r=field(s,result,"Reward",&list);
    if(!r)r=value(s,list,"get_Count",SILPH_READ_I32,&count);if(!r&&count>64)r=SILPH_MANAGED_CONTRACT;
    for(uint32_t i=0;!r&&i<count;i++){
        r=item(s,list,i,&loot);if(!r)r=silph_read_is_type(s->read,loot,"Holoholo.Rpc","LootProto");if(!r)r=field(s,loot,"LootItem",&entries);
        uint64_t n=0;if(!r)r=value(s,entries,"get_Count",SILPH_READ_I32,&n);if(!r&&n>64)r=SILPH_MANAGED_CONTRACT;
        for(uint32_t j=0;!r&&j<n;j++){
            uint64_t kind=0,amount=0;r=item(s,entries,j,&entry);if(!r)r=silph_read_is_type(s->read,entry,"Holoholo.Rpc","LootItemProto");
            if(!r)r=value(s,entry,"get_TypeCase",SILPH_READ_ENUM32,&kind);if(!r)r=silph_reward_integer(s->read,entry,"Count",&amount);
            if(!r){if(kind==4)m.candies+=amount;else if(kind==11)m.xl+=amount;else if(kind==6)m.xp+=amount;else if(kind==2)m.stardust+=amount;else if(kind==1){uint64_t id=0;r=value(s,entry,"get_Item",SILPH_READ_ENUM32,&id);if(!r&&id==1301)m.rare+=amount;else if(!r&&id==1302)m.rare_xl+=amount;}}
            int d=drop(s,&entry);if(!r)r=d;
        }
        int a=drop(s,&entries),b=drop(s,&loot);if(!r)r=a?a:b;
    }
    int a=drop(s,&entry),b=drop(s,&entries),c=drop(s,&loot),d=drop(s,&list);if(!r)r=a?a:b?b:c?c:d;
    if(!r){s->metrics.candies+=m.candies;s->metrics.rare+=m.rare;s->metrics.xl+=m.xl;s->metrics.rare_xl+=m.rare_xl;s->metrics.xp+=m.xp;s->metrics.stardust+=m.stardust;*repeat=m.candies||m.rare||m.xl||m.rare_xl;*rare=m.rare||m.rare_xl;}return r;
}
static int finish(Events *s,Pending *p,uint64_t now,uint64_t wall){
    SilphPromiseMetrics m={0};int r=silph_managed_promise_poll(p->promise,&s->identity,now,&m);if(r)return r;
    if(m.state==SILPH_PROMISE_WAITING)return 0;
    uint64_t status=0;bool fresh=false;SilphManagedRef result={0};
    if(m.state==SILPH_PROMISE_RESOLVED){r=silph_managed_promise_take(p->promise,&s->identity,now,&result,&fresh);SilphReadValue v={0};if(!r&&!fresh)r=SILPH_MANAGED_STALE;if(!r)r=silph_read_field(s->read,result,"Status",SILPH_READ_ENUM32,&v);status=v.bits;
        if(!r&&status==1){s->metrics.opened++;bool repeat=false,rare=false;int parsed=rewards(s,result,&repeat,&rare);if(parsed)s->metrics.reward_error=(uint32_t)parsed;SilphManagedRef encounter={0};int e=field(s,result,"Encounter",&encounter);if(!e&&encounter.serial)s->metrics.encounters++;drop(s,&encounter);
            if(!parsed&&repeat){uint64_t elapsed=now>=p->started?(now-p->started)/UINT64_C(1000000):0;silph_event_remember(&s->history,p->cell,p->id,silph_event_repeat_hold(wall,elapsed,p->expires));s->rearmed++;}
            if(!parsed&&rare)silph_event_rare_remember(&s->priority,p->cell,p->id,p->index,silph_event_hold(wall,p->expires));else silph_event_rare_forget(&s->priority,p->cell,p->id);}
        else if(!r){s->metrics.failed++;silph_event_remember(&s->history,p->cell,p->id,wall+UINT64_C(30000));if(status!=2&&status!=4)s->paused_until=now+UINT64_C(1000000000);}
    }else{s->metrics.failed++;s->paused_until=now+UINT64_C(1000000000);}
    int a=drop(s,&result),b=silph_managed_promise_close(p->promise);if(!r)r=a?a:b;
    if(!b){free(p->promise);*p=(Pending){0};if(s->metrics.pending)s->metrics.pending--;}
    if(r||status!=1)__android_log_print(ANDROID_LOG_INFO,"SilphEvent","box result=%llu promise=%u error=%d",(unsigned long long)status,m.state,r);
    return r;
}
static int process(Events *s,const SilphSchedulePermit *permit,SilphManagedRef object,uint32_t index,SilphEventRareBox *rare,uint64_t now,uint64_t wall){
    uint64_t cell=0,id=0,expiry=0;int r=value(s,object,"get_EncounterId",SILPH_READ_U64,&id);if(!r)r=value(s,object,"get_CellId",SILPH_READ_U64,&cell);if(r)return r;
    if(rare&&(rare->cell!=cell||rare->id!=id)){rare->index=UINT32_MAX;return 0;}
    SilphEventRareBox *known=silph_event_rare_find(&s->priority,cell,id);if(known)known->index=index;
    if(silph_event_seen(&s->history,cell,id,wall))return 0;
    for(unsigned i=0;i<SILPH_EVENT_PENDING;i++)if(s->pending[i].promise&&s->pending[i].id==id&&s->pending[i].cell==cell)return 0;
    r=value(s,object,"get_DespawnTimeMs",SILPH_READ_I64,&expiry);if(r)return r;if((int64_t)expiry>0&&expiry<=wall){silph_event_rare_forget(&s->priority,cell,id);return 0;}
    SilphManagedRef key={0};SilphReadValue v={0};char type[160]={0};uint32_t length=0;r=call(s,object,"get_TypeKey",SILPH_READ_REFERENCE,NULL,0,&v);key=v.reference;
    if(!r)r=silph_read_utf8(s->read,key,type,sizeof(type),&length);int d=drop(s,&key);if(!r)r=d;if(r||strcmp(type,"TAPPABLE_TYPE_BOX")){if(!r)silph_event_rare_forget(&s->priority,cell,id);return r;}
    Pending *p=NULL;for(unsigned i=0;i<SILPH_EVENT_PENDING;i++)if(!s->pending[i].promise){p=&s->pending[i];break;}if(!p)return 0;
    void *tracker=calloc(1,silph_managed_promise_size());if(!tracker)return SILPH_MANAGED_CAPACITY;
    silph_event_remember(&s->history,cell,id,silph_event_hold(wall,expiry));s->metrics.requested++;
    r=silph_event_process(s->read,permit,s->service,object,&v);
    if(!r)r=silph_managed_promise_open_rpc(tracker,silph_managed_promise_size(),s->read,s->managed,v.reference,&s->identity,s->metrics.requested,id,now,"Holoholo.Rpc","ProcessTappableOutProto");
    if(!r){*p=(Pending){tracker,id,cell,expiry,now,index};s->metrics.pending++;if(rare)s->prioritized++;}else{silph_managed_promise_close(tracker);free(tracker);s->metrics.failed++;}
    d=drop(s,&v.reference);return r?r:d;
}
int silph_events_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *id){
    if(!memory||bytes!=sizeof(Events)||!read||!managed||!game||!id||(!id->account_hi&&!id->account_lo))return SILPH_MANAGED_ARGUMENT;
    Events *s=memory;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*id;
    int r=silph_game_read_event_targets(game,&s->service,&s->directory);s->opened=1;return r;
}
int silph_events_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,uint64_t wall,SilphEventMetrics *out){
    Events *s=memory;if(!s||!s->opened||!snapshot||!out||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    uint64_t started=clock_ns();int r=0;
    for(unsigned i=0;!r&&i<SILPH_EVENT_PENDING;i++)if(s->pending[i].promise)r=finish(s,&s->pending[i],now,wall);
    if(!r&&now>=s->next_scan&&now>=s->paused_until&&s->metrics.pending<SILPH_EVENT_PENDING){
        s->next_scan=now+UINT64_C(100000000);SilphManagedRef list={0},object={0};uint64_t count=0;r=field(s,s->directory,"emkz",&list);if(!r)r=value(s,list,"get_Count",SILPH_READ_I32,&count);if(!r&&count>65536)r=SILPH_MANAGED_CONTRACT;
        uint64_t scan_started=clock_ns();
        for(unsigned i=0;!r&&count&&i<SILPH_EVENT_PRIORITY&&s->metrics.pending<SILPH_EVENT_PENDING-1;i++){
            SilphEventRareBox *box=&s->priority.boxes[s->priority.cursor++%SILPH_EVENT_PRIORITY];
            if(!box->id||box->until<=wall||box->index>=count||silph_event_seen(&s->history,box->cell,box->id,wall))continue;
            bool pending=false;for(unsigned j=0;j<SILPH_EVENT_PENDING;j++)if(s->pending[j].promise&&s->pending[j].cell==box->cell&&s->pending[j].id==box->id){pending=true;break;}if(pending)continue;
            uint32_t index=box->index;r=item(s,list,index,&object);s->metrics.examined++;
            if(!r&&object.serial)r=process(s,permit,object,index,box,now,wall);int d=drop(s,&object);if(!r)r=d;
            if(clock_ns()-scan_started>UINT64_C(1500000))break;
        }
        for(unsigned i=0;!r&&count&&i<32&&s->metrics.pending<SILPH_EVENT_PENDING;i++){
            if(s->cursor>=count)s->cursor=0;uint32_t index=s->cursor++;r=item(s,list,index,&object);s->metrics.examined++;
            if(!r&&object.serial)r=process(s,permit,object,index,NULL,now,wall);int d=drop(s,&object);if(!r)r=d;
            if(clock_ns()-scan_started>UINT64_C(1500000))break;if(i+1>=count)break;
        }
        int a=drop(s,&object),b=drop(s,&list);if(!r)r=a?a:b;s->metrics.scans++;
    }
    uint64_t spent=clock_ns()-started;s->metrics.work_ns+=spent;if(spent>s->metrics.max_work_ns)s->metrics.max_work_ns=spent;
    if(r){s->metrics.error=(uint32_t)r;s->paused_until=now+UINT64_C(5000000000);}
    if(now>=s->next_log){s->next_log=now+UINT64_C(5000000000);unsigned known=0;for(unsigned i=0;i<SILPH_EVENT_PRIORITY;i++)if(s->priority.boxes[i].id&&s->priority.boxes[i].until>wall)known++;
        __android_log_print(ANDROID_LOG_INFO,"SilphEvent","boxes=%llu requests=%llu candies=%llu rare=%llu xl=%llu rareXL=%llu encountersSkipped=%llu failed=%llu pending=%u error=%u rewardError=%u scans=%llu examined=%llu maxWorkNs=%llu rearmed=%llu rareKnown=%u prioritized=%llu",(unsigned long long)s->metrics.opened,(unsigned long long)s->metrics.requested,(unsigned long long)s->metrics.candies,(unsigned long long)s->metrics.rare,(unsigned long long)s->metrics.xl,(unsigned long long)s->metrics.rare_xl,(unsigned long long)s->metrics.encounters,(unsigned long long)s->metrics.failed,s->metrics.pending,s->metrics.error,s->metrics.reward_error,(unsigned long long)s->metrics.scans,(unsigned long long)s->metrics.examined,(unsigned long long)s->metrics.max_work_ns,(unsigned long long)s->rearmed,known,(unsigned long long)s->prioritized);}
    *out=s->metrics;return r==SILPH_MANAGED_STALE||r==SILPH_MANAGED_THREAD?r:0;
}
int silph_events_close(void *memory){Events *s=memory;if(!s||!s->opened)return 0;int r=0;for(unsigned i=0;i<SILPH_EVENT_PENDING;i++)if(s->pending[i].promise){int e=silph_managed_promise_close(s->pending[i].promise);if(!r)r=e;if(!e){free(s->pending[i].promise);s->pending[i]=(Pending){0};}}
    int a=drop(s,&s->service),b=drop(s,&s->directory);if(!r)r=a?a:b;__android_log_print(ANDROID_LOG_INFO,"SilphEvent","event session closed requested=%llu opened=%llu failed=%llu error=%d",(unsigned long long)s->metrics.requested,(unsigned long long)s->metrics.opened,(unsigned long long)s->metrics.failed,r);if(!r)memset(s,0,sizeof(*s));return r;}
