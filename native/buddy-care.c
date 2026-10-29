#include "buddy-care.h"
#include "gift-actions.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <android/log.h>
typedef struct {
    void *read,*managed,*game;SilphScheduleIdentity identity;SilphBuddyCareOptions options;
    SilphManagedRef service,rpc,stats,cost,bag,master,pokemon,promise;
    uint64_t id,pending_id,due,feed_due,mega_due,stats_due,started,checks,feeds,megas;uint32_t opened,phase,error,operation,form,reserve;
} Care;
size_t silph_buddy_care_size(void){return sizeof(Care);}
static int drop(Care *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)*ref=(SilphManagedRef){0};return r;}
static int scalar(Care *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,kind,&v);if(!r)*out=v.bits;else __android_log_print(ANDROID_LOG_INFO,"SilphBuddy","field=%s error=%d",name,r);return r;}
static int ref_call(Care *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_call(s->read,ref,name,SILPH_READ_REFERENCE,NULL,0,&v);if(!r)*out=v.reference;else __android_log_print(ANDROID_LOG_INFO,"SilphBuddy","reference=%s error=%d",name,r);return r;}
static int ref_field(Care *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;else __android_log_print(ANDROID_LOG_INFO,"SilphBuddy","reference=%s error=%d",name,r);return r;}
static int flag(Care *s,SilphManagedRef ref,const char *name,bool *out){SilphReadValue v={0};int r=silph_read_call(s->read,ref,name,SILPH_READ_BOOL,NULL,0,&v);if(!r)*out=v.bits!=0;return r;}
static uint64_t wall_ms(void){struct timespec t={0};clock_gettime(CLOCK_REALTIME,&t);return(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static int buddy(Care *s,bool *changed){
    *changed=false;bool has=false;int r=flag(s,s->service,"get_HasBuddy",&has);SilphManagedRef pokemon={0};uint64_t id=0;
    if(!r&&has)r=ref_call(s,s->service,"get_Pokemon",&pokemon);
    if(!r&&pokemon.serial)r=scalar(s,pokemon,"id_",SILPH_READ_U64,&id);
    if(!r&&has&&!id)r=SILPH_MANAGED_NOT_READY;
    if(!r){*changed=id!=s->id;drop(s,&s->pokemon);s->pokemon=pokemon;pokemon=(SilphManagedRef){0};s->id=id;}
    drop(s,&pokemon);return r;
}
static int pending(Care *s,SilphReadValue *v,uint32_t phase,uint64_t now){
    if(!v->reference.serial)return SILPH_MANAGED_CONTRACT;s->promise=v->reference;v->reference=(SilphManagedRef){0};s->phase=phase;s->started=now;s->pending_id=s->id;return 0;
}
static int poll(Care *s,uint64_t now){
    uint64_t complete=0,failed=0;int r=scalar(s,s->promise,"completeCalled",SILPH_READ_BOOL,&complete);
    if(!r)r=scalar(s,s->promise,"errorCalled",SILPH_READ_BOOL,&failed);
    if(!r&&!complete&&!failed&&now-s->started<UINT64_C(15000000000))return 0;
    if(!r&&(!complete||failed))r=SILPH_MANAGED_NOT_READY;
    SilphManagedRef result={0};uint64_t code=0;
    if(!r)r=ref_field(s,s->promise,"completedValue",&result);
    if(!r)r=silph_read_is_type(s->read,result,"Holoholo.Rpc",s->phase==1?"BuddyStatsOutProto":s->phase==2?"BuddyFeedingOutProto":"MegaEvolvePokemonOutProto");
    if(!r)r=scalar(s,result,"Result",SILPH_READ_ENUM32,&code);
    if(!r&&code==1){if(s->phase==2)s->feeds++;if(s->phase==3)s->megas++;}
    __android_log_print(ANDROID_LOG_INFO,"SilphBuddy","operation=%u result=%llu error=%d feeds=%llu megas=%llu",s->phase,(unsigned long long)code,r,(unsigned long long)s->feeds,(unsigned long long)s->megas);
    if(s->phase==2)s->feed_due=now+(r||code!=1?UINT64_C(30000000000):UINT64_C(2000000000));
    if(s->phase==3)s->mega_due=now+(r?UINT64_C(300000000000):UINT64_C(60000000000));
    if(s->phase==1)s->feed_due=now+(r||code!=1?UINT64_C(30000000000):0);
    drop(s,&result);drop(s,&s->promise);s->phase=0;s->due=now;s->error=(uint32_t)r;return 0;
}
static int need_food(Care *s,bool *need){
    *need=false;SilphReadValue remaining={0},total={0};int r=silph_read_call(s->read,s->service,"get_BuddyOnMapSecondsRemaining",SILPH_READ_F32,NULL,0,&remaining);
    if(!r)r=silph_read_call(s->read,s->service,"get_BuddyOnMapSecondsTotal",SILPH_READ_F32,NULL,0,&total);
    if(r)__android_log_print(ANDROID_LOG_INFO,"SilphBuddy","remaining-time error=%d",r);
    float left=0,full=0;memcpy(&left,&remaining.bits,4);memcpy(&full,&total.bits,4);
    if(!r&&(!isfinite(left)||!isfinite(full)||full<0))r=SILPH_MANAGED_CONTRACT;
    if(left<0)left=0;
    if(!r&&full>0){*need=left<=full*(2.0f/3.0f);return 0;}
    SilphManagedRef stats={0},feed={0};if(!r)r=ref_call(s,s->stats,"get_BuddyStats",&stats);
    if(!r&&!stats.serial)r=SILPH_MANAGED_NOT_READY;if(!r)r=ref_field(s,stats,"BuddyFeedStats",&feed);
    uint64_t expiration=0;if(!r&&!feed.serial)r=SILPH_MANAGED_NOT_READY;
    if(!r)r=scalar(s,feed,"MapExpirationMs",SILPH_READ_I64,&expiration);
    if(!r)*need=expiration<=wall_ms();drop(s,&feed);drop(s,&stats);return r;
}
static int feed(Care *s,const SilphSchedulePermit *p,const SilphGameSnapshot *snapshot,uint64_t now){
    bool hungry=false;s->feed_due=now+UINT64_C(30000000000);int r=need_food(s,&hungry);s->reserve=!r&&hungry;if(r||!hungry)return r;
    const uint32_t ids[]={701,703,705,706,708};uint32_t berry=0;
    for(unsigned i=0;i<s->options.count&&!berry;i++)for(unsigned j=0;j<5;j++)if(s->options.berries[i]==ids[j]&&snapshot->items[j+3])berry=ids[j];
    if(!berry){s->feed_due=now+UINT64_C(10000000000);return 0;}
    uint32_t available=0;r=silph_game_read_item(s->game,(int32_t)berry,&available);if(r||!available)return r;
    bool changed=false;r=buddy(s,&changed);if(r||changed||!s->id){s->feed_due=now;return r;}
    SilphReadArgument args[]={{.kind=SILPH_READ_ENUM32,.value.bits=berry},{.kind=SILPH_READ_I32,.value.bits=1}};SilphReadValue v={0};
    if(!r)r=silph_buddy_care_invoke(s->read,p,1,s->rpc,args,2,&v);if(!r)r=pending(s,&v,2,now);drop(s,&v.reference);return r;
}
static int mega(Care *s,const SilphSchedulePermit *p,uint64_t now){
    s->mega_due=now+UINT64_C(60000000000);bool active=false;int r=flag(s,s->pokemon,"get_IsMegaEvolved",&active);if(r||active)return r;
    if(!s->master.serial)r=silph_game_read_service(s->game,0,&s->master);
    SilphReadValue v={0};SilphReadArgument master={.kind=SILPH_READ_REFERENCE,.value.reference=s->master};
    if(!r)r=silph_buddy_care_invoke(s->read,p,7,s->pokemon,&master,1,&v);if(r||!v.bits)return r;
    if(!s->cost.serial)r=silph_game_read_buddy_service(s->game,3,&s->cost);
    if(!r&&!s->bag.serial)r=silph_game_read_buddy_service(s->game,4,&s->bag);
    SilphReadArgument pokemon={.kind=SILPH_READ_REFERENCE,.value.reference=s->pokemon};
    if(!r)r=silph_buddy_care_invoke(s->read,p,6,s->cost,&pokemon,1,&v);if(!r&&!v.bits)return 0;
    SilphManagedRef branches={0},iterator={0},branch={0},settings={0};
    if(!r)r=silph_buddy_care_invoke(s->read,p,3,s->pokemon,&master,1,&v);if(!r)branches=v.reference;
    if(!r&&!branches.serial)r=SILPH_MANAGED_NOT_READY;
    if(!r)r=ref_call(s,branches,"GetEnumerator",&iterator);
    SilphReadArgument settings_args[]={master,{.kind=SILPH_READ_BOOL,.value.bits=0}};
    if(!r)r=silph_buddy_care_invoke(s->read,p,4,s->pokemon,settings_args,2,&v);if(!r)settings=v.reference;
    uint32_t chosen=0;int32_t lowest=INT32_MAX;
    for(unsigned i=0;!r&&i<32;i++){
        r=silph_gift_enumerate(s->read,iterator,1,&v);if(r||!v.bits)break;
        r=silph_gift_enumerate(s->read,iterator,2,&v);if(!r)branch=v.reference;
        if(!r)r=silph_read_is_type(s->read,branch,"Holoholo.Rpc","EvolutionBranchProto");uint64_t form=0;
        if(!r)r=scalar(s,branch,"TemporaryEvolution",SILPH_READ_ENUM32,&form);
        int32_t cost=-1;if(!r&&form)r=silph_buddy_care_cost(s->read,p,s->cost,s->pokemon,branch,&cost);
        if(!r&&form&&cost>=0&&cost<lowest){
            SilphReadArgument args[]={pokemon,{.kind=SILPH_READ_REFERENCE,.value.reference=branch},{.kind=SILPH_READ_REFERENCE,.value.reference=settings}};
            r=silph_buddy_care_invoke(s->read,p,5,s->cost,args,3,&v);if(!r&&v.bits){lowest=cost;chosen=(uint32_t)form;}
        }
        drop(s,&branch);
    }
    if(iterator.serial){silph_gift_enumerate(s->read,iterator,3,&v);drop(s,&v.reference);}
    drop(s,&branch);drop(s,&iterator);drop(s,&branches);drop(s,&settings);if(r||!chosen)return r;
    uint64_t expected=s->id;bool changed=false;r=buddy(s,&changed);if(r||changed||s->id!=expected)return r;
    s->form=chosen;r=silph_buddy_care_mega(s->read,p,s->bag,s->pokemon,s->id,chosen,&v);if(!r)r=pending(s,&v,3,now);drop(s,&v.reference);
    if(!r)__android_log_print(ANDROID_LOG_INFO,"SilphBuddy","mega requested form=%u energy=%d",chosen,lowest);return r;
}
int silph_buddy_care_open(void *memory,size_t size,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,const SilphBuddyCareOptions *options){
    if(!memory||size!=sizeof(Care)||!read||!managed||!game||!identity||!options||options->flags>3||options->count>5)return SILPH_MANAGED_ARGUMENT;
    Care *s=memory;memset(s,0,size);s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->options=*options;s->opened=1;return 0;
}
int silph_buddy_care_tick(void *memory,const SilphSchedulePermit *p,const SilphGameSnapshot *snapshot,uint64_t now,bool boundary,bool *busy){
    Care *s=memory;if(!s||!s->opened||!snapshot||!busy||!silph_schedule_permitted(p,true))return SILPH_MANAGED_STATE;*busy=false;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(s->promise.serial){poll(s,now);*busy=s->promise.serial&&s->phase!=1;return 0;}
    if(!boundary||now<s->due)return 0;s->due=now+UINT64_C(5000000000);s->checks++;int r=0;s->operation=1;
    if(!s->service.serial)r=silph_game_read_buddy_service(s->game,0,&s->service);
    if(!r&&!s->rpc.serial)r=silph_game_read_buddy_service(s->game,1,&s->rpc);
    if(!r&&(s->options.flags&1)&&!s->stats.serial)r=silph_game_read_buddy_service(s->game,2,&s->stats);
    bool changed=false;if(!r)r=buddy(s,&changed);if(!r&&!s->id){s->reserve=0;return 0;}
    if(!r&&changed)s->feed_due=s->mega_due=s->stats_due=now;
    if(!r){
        if((s->options.flags&1)&&now>=s->feed_due){s->operation=3;r=feed(s,p,snapshot,now);}
        if(r==SILPH_MANAGED_NOT_READY&&!s->promise.serial&&now>=s->stats_due){
            SilphReadValue v={0};s->stats_due=now+UINT64_C(30000000000);s->operation=2;r=silph_buddy_care_invoke(s->read,p,2,s->rpc,NULL,0,&v);
            if(!r)r=pending(s,&v,1,now);drop(s,&v.reference);
        }
        if(!s->promise.serial&&(s->options.flags&2)&&now>=s->mega_due){if(r)__android_log_print(ANDROID_LOG_INFO,"SilphBuddy","feeding deferred error=%d",r);s->operation=4;r=mega(s,p,now);}
    }
    if((s->options.flags&1)&&s->feed_due>now&&s->feed_due<s->due)s->due=s->feed_due;
    *busy=s->promise.serial&&s->phase!=1;if(r){s->error=(uint32_t)r;s->due=now+UINT64_C(30000000000);__android_log_print(ANDROID_LOG_INFO,"SilphBuddy","deferred operation=%u error=%d",s->operation,r);}return 0;
}
uint32_t silph_buddy_care_floor(void *memory,const SilphGameSnapshot *snapshot,uint32_t item){
    Care *s=memory;if(!s||!s->opened||!s->id||!s->reserve||!(s->options.flags&1)||!snapshot)return 0;
    const uint32_t ids[]={701,703,705,706,708};uint32_t remaining=3;
    for(unsigned i=0;i<s->options.count&&remaining;i++)for(unsigned j=0;j<5;j++)if(s->options.berries[i]==ids[j]){
        uint32_t count=snapshot->items[j+3],keep=count<remaining?count:remaining;if(item==ids[j])return keep;remaining-=keep;break;
    }
    return 0;
}
int silph_buddy_care_close(void *memory){
    Care *s=memory;if(!s||!s->opened)return 0;SilphManagedRef *refs[]={&s->promise,&s->pokemon,&s->master,&s->bag,&s->cost,&s->stats,&s->rpc,&s->service};int r=0;
    for(unsigned i=0;i<8;i++){int d=drop(s,refs[i]);if(!r)r=d;}if(!r)memset(s,0,sizeof(*s));return r;
}
