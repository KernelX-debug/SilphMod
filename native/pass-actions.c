#define _POSIX_C_SOURCE 200809L
#include "pass-actions.h"
#include "gift-actions.h"
#include "response-rewards.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {char id[257];uint32_t track,rank,terminal;uint64_t until;} Claim;
typedef struct {
    void *read,*managed,*game,*pending;SilphScheduleIdentity identity;SilphPassMetrics metrics;
    SilphManagedRef service,handler,passes,pass,id,encounters,iterator;
    uint32_t opened,index,total,rank,premium,encounter_index,encounter_count,claim_index;
    uint64_t due,sequence;char pass_id[257];Claim claims[256];
} Pass;
size_t silph_pass_size(void){return sizeof(Pass);}
static uint64_t work_clock(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static int drop(Pass *s,SilphManagedRef *r){return silph_reward_drop(s->managed,r);}
static int field(Pass *s,SilphManagedRef ref,const char *name,uint32_t kind,SilphReadValue *out){return silph_read_field(s->read,ref,name,kind,out);}
static int count(Pass *s,SilphManagedRef list,uint32_t max,uint32_t *out){SilphReadValue v={0};int r=silph_read_call(s->read,list,"get_Count",SILPH_READ_I32,NULL,0,&v);if(!r&&v.bits>max)r=SILPH_MANAGED_CONTRACT;if(!r)*out=(uint32_t)v.bits;return r;}
static int item(Pass *s,SilphManagedRef list,uint32_t index,const char *type,SilphManagedRef *out){SilphReadValue v={0};SilphReadArgument a={.kind=SILPH_READ_I32,.value.bits=index};int r=silph_read_call(s->read,list,"get_Item",SILPH_READ_REFERENCE,&a,1,&v);if(!r)r=silph_read_is_type(s->read,v.reference,"Holoholo.Rpc",type);if(!r)*out=v.reference;else drop(s,&v.reference);return r;}
static int call(Pass *s,const SilphSchedulePermit *p,uint32_t op,const SilphReadArgument *args,uint32_t n,SilphReadValue *out){s->metrics.operation=op;return silph_pass_invoke(s->read,p,op,s->service,args,n,out);}
static void dispose(Pass *s){if(s->iterator.serial){SilphReadValue v={0};silph_pass_enumerate(s->read,s->iterator,3,&v);drop(s,&s->iterator);}}
static void next(Pass *s,uint64_t now,bool refresh){dispose(s);drop(s,&s->encounters);drop(s,&s->id);drop(s,&s->pass);s->pass_id[0]=0;s->metrics.state=refresh?1:2;s->due=now+UINT64_C(200000000);if(refresh)drop(s,&s->passes);}
static Claim *claim(Pass *s,uint32_t track,uint32_t rank,uint64_t now){
    Claim *empty=NULL;for(unsigned i=0;i<256;i++){Claim *c=&s->claims[i];if(!c->id[0]&&!empty)empty=c;else if(c->id[0]&&c->track==track&&c->rank==rank&&!strcmp(c->id,s->pass_id))return !c->terminal&&now>=c->until?c:NULL;}return empty;
}
static int send(Pass *s,const SilphSchedulePermit *p,uint32_t track,uint32_t rank,bool all,uint64_t now){
    Claim *c=claim(s,track,rank,now);if(!c)return 0;memset(c,0,sizeof(*c));memcpy(c->id,s->pass_id,strlen(s->pass_id)+1);c->track=track;c->rank=rank;c->terminal=1;s->claim_index=(uint32_t)(c-s->claims);
    SilphReadValue promise={0};s->metrics.operation=all?21:20;int r=silph_pass_claim(s->read,p,s->handler,s->id,track,rank,all,&promise);
    if(!r){s->metrics.requests++;uint64_t sequence=++s->sequence;r=silph_managed_promise_open_rpc(s->pending,silph_managed_promise_size(),s->read,s->managed,promise.reference,&s->identity,sequence,sequence,now,"Holoholo.Rpc","ClaimEventPassRewardsResponseProto");}
    drop(s,&promise.reference);if(!r){s->metrics.state=6;s->due=now+UINT64_C(100000000);}return r;
}
static int poll(Pass *s,uint64_t now){
    SilphPromiseMetrics m={0};int r=silph_managed_promise_poll(s->pending,&s->identity,now,&m);if(!r&&m.state==SILPH_PROMISE_WAITING)return 0;if(!r&&m.state!=SILPH_PROMISE_RESOLVED)r=SILPH_MANAGED_NOT_READY;
    SilphManagedRef result={0};bool fresh=false;if(!r)r=silph_managed_promise_take(s->pending,&s->identity,now,&result,&fresh);if(!r&&!fresh)r=SILPH_MANAGED_STALE;
    SilphReadValue code={0};if(!r)r=field(s,result,"Status",SILPH_READ_ENUM32,&code);if(!r&&code.bits>7)r=SILPH_MANAGED_CONTRACT;s->metrics.result=(uint32_t)code.bits;
    if(!r&&code.bits==1){s->metrics.claimed++;uint64_t xp=0,dust=0;int rewards=silph_quest_reward_list(s->read,s->managed,result,"ClaimEventPassRewardsResponseProto","RewardsGranted",&xp,&dust);if(!rewards){s->metrics.xp+=xp;s->metrics.stardust+=dust;}else s->metrics.error=(uint32_t)rewards;}
    if(!r&&code.bits==2){Claim *c=&s->claims[s->claim_index];c->terminal=0;c->until=now+UINT64_C(30000000000);}
    drop(s,&result);int closed=silph_managed_promise_close(s->pending);if(!r)r=closed;next(s,now,true);s->due=now+UINT64_C(500000000);return r;
}
static int begin_pass(Pass *s,const SilphSchedulePermit *p){
    if(s->index>=s->total){drop(s,&s->passes);s->metrics.state=1;return 0;}int r=item(s,s->passes,s->index++,"EventPassStateProto",&s->pass);SilphReadValue id={0},rank={0},premium={0};uint32_t length=0;
    if(!r)r=field(s,s->pass,"PassId",SILPH_READ_REFERENCE,&id);if(!r)r=silph_read_utf8(s->read,id.reference,s->pass_id,257,&length);if(!r&&(!length||length>256))r=SILPH_MANAGED_CONTRACT;
    if(!r)r=field(s,s->pass,"CurrentRank",SILPH_READ_I32,&rank);if(!r&&rank.bits>10000)r=SILPH_MANAGED_CONTRACT;
    if(!r){s->id=id.reference;id.reference=(SilphManagedRef){0};s->rank=(uint32_t)rank.bits;SilphReadArgument a={.kind=SILPH_READ_REFERENCE,.value.reference=s->id};r=call(s,p,5,&a,1,&premium);s->premium=!r&&premium.bits;}
    SilphReadValue encounters={0};if(!r)r=field(s,s->pass,"Encounters",SILPH_READ_REFERENCE,&encounters);if(!r)r=count(s,encounters.reference,1024,&s->encounter_count);
    if(!r){s->encounters=encounters.reference;encounters.reference=(SilphManagedRef){0};s->encounter_index=0;s->metrics.state=3;}
    drop(s,&id.reference);drop(s,&encounters.reference);return r;
}
static int cached(Pass *s,bool capture,SilphManagedRef *reward){
    if(!capture){s->metrics.state=4;return 0;}
    uint32_t current=0;int checked=count(s,s->encounters,1024,&current);if(checked)return checked;if(current<s->encounter_count)s->encounter_index=0;s->encounter_count=current;
    for(unsigned step=0;step<2&&s->encounter_index<s->encounter_count;step++){
        SilphManagedRef candidate={0};SilphReadValue type={0},ball={0};int r=item(s,s->encounters,s->encounter_index++,"QuestPokemonEncounterProto",&candidate);
        if(!r)r=field(s,candidate,"EncounterType",SILPH_READ_ENUM32,&type);if(!r)r=field(s,candidate,"PokeBallOverride",SILPH_READ_ENUM32,&ball);
        if(!r&&type.bits==25&&!ball.bits){*reward=candidate;s->metrics.encounters++;return 0;}drop(s,&candidate);if(r)return r;
    }
    if(s->encounter_index>=s->encounter_count)s->metrics.state=4;return 0;
}
static int items(Pass *s,const SilphSchedulePermit *p,uint64_t now){
    SilphReadArgument a={.kind=SILPH_READ_REFERENCE,.value.reference=s->id};SilphReadValue counts={0},n={0};int r=call(s,p,2,&a,1,&counts);
    if(!r)r=silph_read_is_type(s->read,counts.reference,"Niantic.Holoholo.UI.TodayView.EventPass","EventPassRewards");
    if(!r)r=silph_read_call(s->read,counts.reference,"get_Items",SILPH_READ_I32,NULL,0,&n);drop(s,&counts.reference);if(!r&&n.bits>10000000)r=SILPH_MANAGED_CONTRACT;
    if(!r&&n.bits&&claim(s,0,s->rank,now))return send(s,p,0,s->rank,true,now);
    if(!r){SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=s->pass};SilphReadValue enumerable={0},it={0};r=call(s,p,3,&arg,1,&enumerable);
        if(!r)r=silph_pass_enumerate(s->read,enumerable.reference,4,&it);if(!r){s->iterator=it.reference;it.reference=(SilphManagedRef){0};s->metrics.state=5;}drop(s,&enumerable.reference);drop(s,&it.reference);}
    return r;
}
static int slots(Pass *s,const SilphSchedulePermit *p,uint64_t now,bool capture){
    if(!capture){next(s,now,false);return 0;}
    for(unsigned step=0;step<2;step++){
        SilphReadValue more={0},tuple={0},track={0},rank={0},details={0},ball={0},claimed={0};int r=silph_pass_enumerate(s->read,s->iterator,1,&more);
        if(!r&&!more.bits){next(s,now,false);return 0;}if(!r)r=silph_pass_enumerate(s->read,s->iterator,2,&tuple);
        if(!r)r=field(s,tuple.reference,"Item1",SILPH_READ_ENUM32,&track);if(!r)r=field(s,tuple.reference,"Item2",SILPH_READ_I32,&rank);
        if(!r)r=field(s,tuple.reference,"Item3",SILPH_READ_REFERENCE,&details);if(!r)r=silph_read_is_type(s->read,details.reference,"Holoholo.Rpc","PokemonEncounterRewardProto");
        if(!r)r=field(s,details.reference,"PokeBallOverride",SILPH_READ_ENUM32,&ball);
        bool eligible=!r&&(track.bits==1||(track.bits==2&&s->premium))&&rank.bits<=s->rank&&!ball.bits;
        if(eligible){SilphReadArgument a[]={{.kind=SILPH_READ_REFERENCE,.value.reference=s->id},{.kind=SILPH_READ_ENUM32,.value.bits=track.bits},{.kind=SILPH_READ_I32,.value.bits=rank.bits}};r=call(s,p,4,a,3,&claimed);}
        drop(s,&details.reference);drop(s,&tuple.reference);if(r)return r;
        if(eligible&&!claimed.bits&&claim(s,(uint32_t)track.bits,(uint32_t)rank.bits,now))return send(s,p,(uint32_t)track.bits,(uint32_t)rank.bits,false,now);
    }return 0;
}
int silph_pass_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity){
    if(!memory||bytes!=sizeof(Pass)||!read||!managed||!game||!identity)return SILPH_MANAGED_ARGUMENT;Pass *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->opened=1;
    s->pending=calloc(1,silph_managed_promise_size());int r=s->pending?0:SILPH_MANAGED_CAPACITY;if(!r)r=silph_game_read_research_service(game,4,&s->service);if(!r)r=silph_game_read_rpc_handler(game,&s->handler);s->metrics.state=r?7:1;s->metrics.error=(uint32_t)r;return 0;
}
int silph_pass_tick(void *memory,const SilphSchedulePermit *p,const SilphGameSnapshot *snapshot,uint64_t now,bool capture,bool accept,SilphManagedRef *reward,SilphPassMetrics *out){
    Pass *s=memory;if(!s||!s->opened||!snapshot||!reward||!out||!silph_schedule_permitted(p,true))return SILPH_MANAGED_STATE;*reward=(SilphManagedRef){0};if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(now<s->due||s->metrics.state==7){*out=s->metrics;return 0;}uint64_t began=work_clock();s->due=now+UINT64_C(100000000);int r=0;
    if(s->metrics.state==1){SilphReadValue list={0};r=call(s,p,1,NULL,0,&list);if(!r)r=count(s,list.reference,32,&s->total);if(!r){s->passes=list.reference;list.reference=(SilphManagedRef){0};s->index=0;s->metrics.state=2;s->metrics.scans++;}drop(s,&list.reference);}
    else if(s->metrics.state==2){r=begin_pass(s,p);if(!r&&s->metrics.state==1)s->due=now+UINT64_C(15000000000);}
    else if(s->metrics.state==3)r=cached(s,capture&&accept,reward);
    else if(s->metrics.state==4)r=items(s,p,now);
    else if(s->metrics.state==5)r=slots(s,p,now,capture);
    else if(s->metrics.state==6)r=poll(s,now);
    if(r){s->metrics.error=s->metrics.operation*100+(uint32_t)r;silph_managed_promise_close(s->pending);next(s,now,true);s->due=now+UINT64_C(30000000000);}
    uint64_t elapsed=work_clock()-began;s->metrics.work_ns+=elapsed;if(elapsed>s->metrics.max_work_ns)s->metrics.max_work_ns=elapsed;*out=s->metrics;return 0;
}
int silph_pass_close(void *memory){Pass *s=memory;if(!s||!s->opened)return 0;int r=s->pending?silph_managed_promise_close(s->pending):0;dispose(s);SilphManagedRef *refs[]={&s->encounters,&s->id,&s->pass,&s->passes,&s->service,&s->handler};for(unsigned i=0;i<6;i++){int freed=drop(s,refs[i]);if(!r)r=freed;}if(!r){free(s->pending);memset(s,0,sizeof(*s));}return r;}
