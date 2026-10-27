#include "farmer-actions.h"
#ifdef SILPH_PRESENT_SCOPED
#include "event-actions.h"
#endif
#include "research-bridge.h"
#include "research-policy.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef SILPH_PILOT_SERVICE
#include "response-rewards.h"
#include <android/log.h>
#endif
#include "core.h"
enum {SILPH_CATCH_ERROR=0,SILPH_CATCH_SUCCESS=1,SILPH_CATCH_ESCAPE=2,SILPH_CATCH_FLEE=3,SILPH_CATCH_MISSED=4,SILPH_CATCH_REPLACEMENT=5};

typedef struct {
    void *read,*managed,*game,*pending;
    SilphScheduleIdentity identity;
    SilphGameTarget target;
    SilphFarmerMetrics metrics;
    uint64_t sequence,capture_sequence,capture_sent,owned_started;
    void *scope;
    SilphFarmerOptions options;
    struct {uint64_t id,until;} seen[2048];
    uint64_t clock;
    unsigned opened,transfer_enabled,target_throws,berry_attempted,berry_suppressed,active_item,selected_berry,only_active,source,target_deferred;
} Farmer;
size_t silph_farmer_actions_size(void){return sizeof(Farmer);}
int silph_farmer_actions_metrics(void *memory,SilphFarmerMetrics *out){Farmer *s=memory;if(!s||!s->opened||!out)return SILPH_MANAGED_ARGUMENT;*out=s->metrics;return 0;}
bool silph_farmer_actions_inventory_due(void *memory){
    Farmer *s=memory;return !s||!s->opened||s->metrics.state==SILPH_FARMER_SELECT||s->metrics.state==SILPH_FARMER_RETRY
#if !defined(SILPH_PILOT_COMBINED) || !defined(SILPH_PILOT_SERVICE)
        ||s->metrics.state==SILPH_FARMER_SYNC
#endif
        ;
}
static int release(Farmer *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)memset(ref,0,sizeof(*ref));return r;}
static int drop_target(Farmer *s){
    int status=0;if(s->target.object.serial&&s->source!=SILPH_SOURCE_WILD)status=silph_research_destroy(s->read,s->target.object);int freed=release(s,&s->target.object);return status?status:freed;}
int silph_farmer_research_target(void *memory,SilphManagedRef target,uint64_t encounter,uint32_t species,uint32_t source){
    Farmer *s=memory;if(!s||!s->opened||s->metrics.state!=SILPH_FARMER_SELECT||s->target.object.serial||!encounter||!species||species>=2048)return SILPH_MANAGED_STATE;
    if(source!=SILPH_SOURCE_RESEARCH&&source!=SILPH_SOURCE_GO_PASS)return SILPH_MANAGED_CONTRACT;
    int status=silph_read_is_type(s->read,target,"Niantic.Holoholo.Quests","QuestMapPokemon");SilphReadValue id={0},destroyed={0},type={0};
    if(!status)status=silph_read_call(s->read,target,"get_EncounterId",SILPH_READ_U64,NULL,0,&id);
    if(!status)status=silph_read_call(s->read,target,"get_Destroyed",SILPH_READ_BOOL,NULL,0,&destroyed);
    if(!status)status=silph_read_call(s->read,target,"get_EncounterType",SILPH_READ_ENUM32,NULL,0,&type);
    if(!status&&!silph_research_encounter_type((uint32_t)type.bits))status=SILPH_MANAGED_CONTRACT;
    if(!status&&(id.bits!=encounter||destroyed.bits))status=SILPH_MANAGED_STALE;
    void *object=NULL;if(!status)status=silph_managed_target(s->managed,target,&object);
    if(!status)status=silph_managed_retain(s->managed,object,false,&s->target.object);
    if(!status){s->source=source;s->target.encounter=encounter;s->target.species=species;s->metrics.target_id=encounter;s->metrics.species=species;s->metrics.encounters++;s->metrics.state=SILPH_FARMER_RETRY;}
    return status;
}
int silph_farmer_actions_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,uint32_t transfers){
    if(!memory||bytes!=sizeof(Farmer)||!read||!managed||!game||!identity||transfers>1)return SILPH_MANAGED_ARGUMENT;
    Farmer *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);
    s->pending=calloc(1,silph_managed_promise_size());if(!s->pending)return SILPH_MANAGED_CAPACITY;
    uint32_t scope_bytes=silph_core_size(4);s->scope=calloc(1,scope_bytes);
    if(!s->scope||silph_core_open(s->scope,scope_bytes,4,1)){free(s->pending);free(s->scope);memset(s,0,bytes);return SILPH_MANAGED_CAPACITY;}
    s->transfer_enabled=transfers;s->options=(SilphFarmerOptions){.abi=1,.max_catches=1,.keep_options=255};
    s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->metrics.state=SILPH_FARMER_SELECT;s->opened=1;return 0;
}
int silph_farmer_actions_configure(void *memory,const SilphFarmerOptions *options){
    Farmer *s=memory;if(!s||!s->opened||s->metrics.encounters||s->metrics.state!=SILPH_FARMER_SELECT)return SILPH_MANAGED_STATE;
    if(!options||options->abi!=1||options->continuous>1||!options->max_catches||options->max_catches>SILPH_CORE_MAX_CAPTURES||options->quality<1||options->quality>4
        ||options->curve>1||options->use_berries>1||options->keep_options>255||options->berry_count>5||(options->use_berries&&!options->berry_count))return SILPH_MANAGED_ARGUMENT;
    for(unsigned i=0;i<options->berry_count;i++){
        int32_t v=options->berries[i];if(v!=701&&v!=703&&v!=705&&v!=706&&v!=708)return SILPH_MANAGED_ARGUMENT;
        for(unsigned j=0;j<i;j++)if(v==options->berries[j])return SILPH_MANAGED_ARGUMENT;
    }
    uint32_t capacity=options->max_catches<4?4:options->max_catches,bytes=silph_core_size(capacity);void *scope=calloc(1,bytes);
    if(!scope||silph_core_open(scope,bytes,capacity,0)){free(scope);return SILPH_MANAGED_CAPACITY;}
    if(silph_core_close(s->scope)){silph_core_close(scope);free(scope);return SILPH_MANAGED_STATE;}
    free(s->scope);s->scope=scope;s->options=*options;
    for(unsigned i=0;i<sizeof(options->only_species);i++)if(options->only_species[i])s->only_active=1;return 0;
}
static bool accept_target(void *context,uint64_t id,uint32_t species){
    Farmer *s=context;bool event=false;
#ifdef SILPH_PRESENT_SCOPED
    event=silph_event_owned(id);
#endif
    if(!event&&s->only_active&&(species>=2048||!(s->options.only_species[species/8]&(1u<<(species%8)))))return false;
    unsigned slot=(unsigned)((id^(id>>32))*UINT64_C(11400714819323198485))&2047;
    for(unsigned i=0;i<16;i++){unsigned n=(slot+i)&2047;if(s->seen[n].id==id&&s->seen[n].until>s->clock)return false;}return true;
}
static void remember_target(Farmer *s){
    uint64_t id=s->target.encounter;if(!id)return;unsigned slot=(unsigned)((id^(id>>32))*UINT64_C(11400714819323198485))&2047,chosen=slot;
    for(unsigned i=0;i<16;i++){unsigned n=(slot+i)&2047;if(s->seen[n].id==id||s->seen[n].until<=s->clock){chosen=n;break;}if(s->seen[n].until<s->seen[chosen].until)chosen=n;}
    s->seen[chosen].id=id;s->seen[chosen].until=s->clock+(s->target_deferred?UINT64_C(15000000000):UINT64_C(600000000000));
}
static int continue_session(Farmer *s){
    if(s->metrics.state!=SILPH_FARMER_DONE||!s->options.continuous||s->metrics.catches>=s->options.max_catches)return 0;
    if(s->source==SILPH_SOURCE_WILD)remember_target(s);int status=drop_target(s);if(status)return status;
    memset(&s->target,0,sizeof(s->target));s->target_throws=s->berry_attempted=s->berry_suppressed=s->active_item=s->selected_berry=s->target_deferred=0;
    s->source=SILPH_SOURCE_WILD;s->metrics.scope_granted=0;s->metrics.state=SILPH_FARMER_SELECT;return 0;
}
static int reserve(Farmer *s,SilphManagedRef promise,uint64_t now,const char *space,const char *name){
    int status=silph_managed_promise_open(s->pending,silph_managed_promise_size(),s->read,s->managed,promise,&s->identity,
        ++s->sequence,s->target.encounter,now,space,name);
    int freed=release(s,&promise);return status?status:freed;
}
static int target_valid(Farmer *s){
    SilphReadValue value;int status=silph_read_call(s->read,s->target.object,"get_Destroyed",SILPH_READ_BOOL,NULL,0,&value);
    if(!status&&value.bits)return SILPH_MANAGED_NOT_READY;
    if(!status)status=silph_read_call(s->read,s->target.object,"get_EncounterId",SILPH_READ_U64,NULL,0,&value);
    if(!status&&value.bits!=s->target.encounter)status=SILPH_MANAGED_STALE;
    if(!status&&s->source!=SILPH_SOURCE_WILD)return 0;
    double meters=INFINITY;if(!status)status=silph_game_read_distance(s->game,s->target.object,&meters);
    if(!status&&(!isfinite(meters)||meters>50)){
#ifdef SILPH_PRESENT_SCOPED
        s->target_deferred=1;
#endif
        status=SILPH_MANAGED_NOT_READY;
    }return status;
}
static int throw_ball(Farmer *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now){
    int status=target_valid(s);if(status==SILPH_MANAGED_NOT_READY){s->metrics.state=SILPH_FARMER_DONE;return 0;}if(status)return status;
    if(snapshot->pokemon_count>=snapshot->max_pokemon||(!s->options.continuous&&s->target_throws>=3)){s->metrics.state=SILPH_FARMER_DONE;return 0;}
    unsigned ball=0;for(unsigned i=0;i<3;i++)if(snapshot->items[i]){ball=i+1;break;}
    if(!ball){s->metrics.state=SILPH_FARMER_DONE;return 0;}
    if(s->options.use_berries&&!s->active_item&&!s->berry_attempted&&!s->berry_suppressed){
        const int32_t ids[]={701,703,705,706,708};int32_t berry=0;
        for(unsigned i=0;!berry&&i<s->options.berry_count;i++)for(unsigned j=0;j<5;j++)if(s->options.berries[i]==ids[j]&&snapshot->items[j+3])berry=ids[j];
        if(berry){SilphManagedRef bag={0};SilphReadValue pending={0};s->berry_attempted=1;s->selected_berry=(unsigned)berry;
            status=silph_game_read_item_bag(s->game,&bag);
            SilphReadArgument arguments[]={{.kind=SILPH_READ_ENUM32,.value.bits=(uint32_t)berry},{.kind=SILPH_READ_REFERENCE,.value.reference=s->target.object}};
            if(!status){s->metrics.berry_requests++;status=silph_action_call(s->read,permit,SILPH_ACTION_BERRY,bag,arguments,2,&pending);}
            if(!status){status=reserve(s,pending.reference,now,"Holoholo.Rpc","UseItemEncounterOutProto");s->metrics.state=SILPH_FARMER_BERRY;}
            int freed=release(s,&bag);return status?status:freed;
        }
    }
    SilphManagedRef data={0},ar={0};SilphReadValue promise={0};
    const float profiles[]={.5f,1.1f,.85f,.5f,.05f};
    status=silph_action_capture_data(s->read,permit,(int32_t)ball,profiles[s->options.quality],s->options.quality!=0,s->options.curve!=0,&data,&ar);
    SilphReadArgument args[]={{.kind=SILPH_READ_BOXED,.value.reference=data},{.kind=SILPH_READ_REFERENCE,.value.reference=ar}};
    if(!status){s->capture_sent=now/1000000;s->capture_sequence=silph_core_issue(s->scope,s->target.encounter,s->capture_sent,s->source==SILPH_SOURCE_WILD?SILPH_SOURCE_WILD:SILPH_SOURCE_RESEARCH);if(!s->capture_sequence)status=SILPH_MANAGED_STATE;}
    if(!status)status=silph_action_call(s->read,permit,SILPH_ACTION_CAPTURE,s->target.object,args,2,&promise);
    if(!status){s->metrics.throws++;s->target_throws++;status=reserve(s,promise.reference,now,"Holoholo.Rpc","CatchPokemonOutProto");s->metrics.state=SILPH_FARMER_CAPTURE;}
    int a=release(s,&data),b=release(s,&ar);return status?status:a?a:b;
}
static int transfer_new(Farmer *s,const SilphSchedulePermit *permit,uint64_t now){
    uint64_t id=s->metrics.captured_id;
    if(!s->transfer_enabled||!s->metrics.scope_granted||!silph_core_authorize(s->scope,id,s->target.encounter))return SILPH_MANAGED_STATE;
    SilphTransferInput data;SilphManagedRef bag={0},pokemon={0};
    int status=silph_game_read_transfer_data(s->game,id,&data,&bag,&pokemon);
    if(status==SILPH_MANAGED_NOT_READY&&now-s->owned_started<UINT64_C(15000000000))return 0;
    SilphTransferVerdict verdict={0};
    if(!status){data.options=s->options.keep_options;data.source=s->source;if(silph_transfer_evaluate(&data,sizeof(data),&verdict))status=SILPH_MANAGED_CONTRACT;}
    s->metrics.transfer_decision=verdict.decision;s->metrics.transfer_reasons=verdict.reasons;
    if(status||verdict.decision!=SILPH_VERDICT_CANDIDATE){
        s->metrics.transfer_error=status;silph_core_finish(s->scope,id,0);s->metrics.state=SILPH_FARMER_DONE;status=0;
    }else{
        SilphReadValue promise;status=silph_action_release(s->read,permit,s->scope,s->target.encounter,&data,bag,pokemon,&promise);
        if(!status){s->metrics.transfer_requests++;status=reserve(s,promise.reference,now,"Holoholo.Rpc","ReleasePokemonOutProto");s->metrics.state=SILPH_FARMER_TRANSFER;}
    }
    int a=release(s,&bag),b=release(s,&pokemon);return status?status:a?a:b;
}
static void transfer_response(Farmer *s,uint32_t code,uint64_t now){
    if(code==1){s->metrics.state=SILPH_FARMER_SYNC;s->owned_started=now;}
    else{s->metrics.transfer_error=code;silph_core_finish(s->scope,s->metrics.captured_id,0);s->metrics.state=SILPH_FARMER_DONE;}
}
static int transfer_sync(Farmer *s,bool present,uint64_t now){
    if(present&&now-s->owned_started<UINT64_C(15000000000))return 0;
    if(silph_core_finish(s->scope,s->metrics.captured_id,present?0:1))return SILPH_MANAGED_STATE;
    if(present){s->metrics.transfer_error=104;s->metrics.skipped++;}
    else{s->metrics.transferred++;if(s->source==SILPH_SOURCE_RESEARCH)s->metrics.research_transferred++;if(s->source==SILPH_SOURCE_GO_PASS)s->metrics.go_pass_transferred++;}
    s->metrics.scope_granted=0;s->metrics.state=SILPH_FARMER_DONE;return 0;
}
static int terminal_response(Farmer *s,const SilphPromiseMetrics *promise){
    s->metrics.promise_phase=s->metrics.state;s->metrics.promise_state=promise->state;s->metrics.promise_reason=promise->reason;
    bool recoverable=(promise->state==SILPH_PROMISE_FAILED&&promise->reason==SILPH_PROMISE_REASON_NONE)
        ||(promise->state==SILPH_PROMISE_TIMED_OUT&&promise->reason==SILPH_PROMISE_REASON_TIMEOUT)
        ||(promise->state==SILPH_PROMISE_UNCERTAIN&&promise->reason==SILPH_PROMISE_REASON_FLAGS);
    if(!recoverable)return SILPH_MANAGED_STATE;
#ifdef SILPH_PRESENT_SCOPED
    if(s->metrics.state==SILPH_FARMER_ENCOUNTER)s->target_deferred=1;
#ifdef SILPH_PILOT_SERVICE
    __android_log_print(ANDROID_LOG_INFO,"SilphFarmer","request deferred phase=%u state=%u reason=%u encounter=%llu retryEncounter=%u",s->metrics.state,promise->state,promise->reason,(unsigned long long)s->target.encounter,s->target_deferred);
#endif
#endif
    int status=0;
    if(s->metrics.state==SILPH_FARMER_CAPTURE)
        status=silph_core_abandon(s->scope,s->capture_sequence,s->target.encounter,s->capture_sent)?SILPH_MANAGED_STATE:0;
    else if(s->metrics.state==SILPH_FARMER_TRANSFER){
        status=silph_core_finish(s->scope,s->metrics.captured_id,0)?SILPH_MANAGED_STATE:0;
        s->metrics.transfer_error=100+promise->state;
    }
    if(!status)status=silph_managed_promise_close(s->pending);
    if(!status){s->metrics.skipped++;s->metrics.scope_granted=0;s->metrics.state=SILPH_FARMER_DONE;}
    return status;
}
static int capture_response(Farmer *s,SilphManagedRef result,bool fresh,uint64_t now){
    SilphReadValue code={0},id={0};int status=silph_read_field(s->read,result,"Status",SILPH_READ_ENUM32,&code);
    if(!status)status=silph_read_field(s->read,result,"CapturedPokemonId",SILPH_READ_U64,&id);
    if(status)return status;
    s->metrics.status=(uint32_t)code.bits;
    uint64_t proof[]={s->capture_sequence,s->target.encounter,s->capture_sent,now/1000000,id.bits,
        (code.bits==SILPH_CATCH_SUCCESS&&id.bits&&fresh)?15u:0u};
    int accepted=silph_core_settle(s->scope,proof,6);if(accepted<0)return SILPH_MANAGED_STATE;
    if(code.bits>SILPH_CATCH_REPLACEMENT)return SILPH_MANAGED_CONTRACT;
    s->metrics.fresh_response=0;s->metrics.scope_granted=0;
    if(code.bits==SILPH_CATCH_SUCCESS&&id.bits&&fresh&&accepted==1){
#ifdef SILPH_PRESENT_SCOPED
        if(silph_event_owned(s->target.encounter)){
            silph_event_captured(s->target.encounter);
#ifdef SILPH_PILOT_SERVICE
            __android_log_print(ANDROID_LOG_INFO,"SilphEvent","event capture confirmed encounter=%llu species=%u captured=%llu",(unsigned long long)s->target.encounter,s->target.species,(unsigned long long)id.bits);
#endif
        }
#endif
        s->metrics.catches++;if(s->source==SILPH_SOURCE_RESEARCH)s->metrics.research_catches++;if(s->source==SILPH_SOURCE_GO_PASS)s->metrics.go_pass_catches++;
        s->metrics.captured_id=id.bits;s->metrics.fresh_response=1;
#ifdef SILPH_PILOT_SERVICE
        if(s->target.spawn_kind)__android_log_print(ANDROID_LOG_INFO,"SilphFarmer","incense capture confirmed encounter=%llu captured=%llu",(unsigned long long)s->target.encounter,(unsigned long long)id.bits);
#endif
#ifdef SILPH_PILOT_SERVICE
        if(!s->metrics.reward_error){uint64_t xp=0,dust=0;int reward=silph_capture_rewards(s->read,s->managed,result,&xp,&dust);
            if(reward)s->metrics.reward_error=(uint32_t)reward;else{s->metrics.xp+=xp;s->metrics.stardust+=dust;}}
#endif
        s->metrics.scope_granted=1;s->owned_started=now;
        s->metrics.state=s->transfer_enabled?SILPH_FARMER_OWNED:SILPH_FARMER_DONE;
    }else if(code.bits==SILPH_CATCH_ESCAPE&&!id.bits){
        s->active_item=0;s->berry_attempted=0;s->metrics.state=SILPH_FARMER_RETRY;
    }else if(code.bits==SILPH_CATCH_MISSED&&!id.bits){
        s->metrics.state=SILPH_FARMER_RETRY;
    }else if(code.bits==SILPH_CATCH_FLEE&&!id.bits){
        s->metrics.fled++;s->metrics.state=SILPH_FARMER_DONE;
    }else{
        s->metrics.skipped++;s->target_deferred=code.bits!=SILPH_CATCH_REPLACEMENT||id.bits;
        s->metrics.state=SILPH_FARMER_DONE;
    }
    return 0;
}
int silph_farmer_actions_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,SilphFarmerMetrics *output){
    if(!output||!snapshot)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));Farmer *s=memory;
    if(!s||!s->opened||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(!s->identity.account_hi&&!s->identity.account_lo){s->identity.account_hi=snapshot->account_hi;s->identity.account_lo=snapshot->account_lo;}
    if(s->identity.account_hi!=snapshot->account_hi||s->identity.account_lo!=snapshot->account_lo)return SILPH_MANAGED_STALE;
    int status=0;s->clock=now;
    if(s->metrics.state==SILPH_FARMER_SELECT){
        if(s->options.continuous&&(snapshot->pokemon_count>=snapshot->max_pokemon||!(snapshot->items[0]||snapshot->items[1]||snapshot->items[2]))){*output=s->metrics;return 0;}
        status=silph_game_read_select(s->game,50,accept_target,s,&s->target);
        if(status==SILPH_MANAGED_NOT_READY){*output=s->metrics;return 0;}
        if(!status){s->source=SILPH_SOURCE_WILD;s->metrics.target_id=s->target.encounter;s->metrics.species=s->target.species;
#ifdef SILPH_PILOT_SERVICE
            if(s->target.spawn_kind){SilphReadValue item={0};int source=silph_read_call(s->read,s->target.object,"get_SourceType",SILPH_READ_ENUM32,NULL,0,&item);__android_log_print(ANDROID_LOG_INFO,"SilphFarmer","incense encounter species=%u encounter=%llu item=%llu sourceRead=%d",s->target.species,(unsigned long long)s->target.encounter,(unsigned long long)item.bits,source);}
#endif
            SilphReadValue promise;status=silph_action_call(s->read,permit,SILPH_ACTION_ENCOUNTER,s->target.object,NULL,0,&promise);
            if(!status){s->metrics.encounters++;status=reserve(s,promise.reference,now,"Niantic.Holoholo.Map","PokemonEncounterResponse");s->metrics.state=SILPH_FARMER_ENCOUNTER;}
        }
    }else if(s->metrics.state==SILPH_FARMER_RETRY)status=throw_ball(s,permit,snapshot,now);
    else if(s->metrics.state==SILPH_FARMER_OWNED)status=transfer_new(s,permit,now);
    else if(s->metrics.state==SILPH_FARMER_SYNC){
        SilphManagedRef pokemon={0};status=silph_game_read_owned(s->game,s->metrics.captured_id,&pokemon);
        if(!status)status=transfer_sync(s,pokemon.serial!=0,now);
        int freed=release(s,&pokemon);if(!status)status=freed;
    }else if(s->metrics.state==SILPH_FARMER_ENCOUNTER||s->metrics.state==SILPH_FARMER_CAPTURE||s->metrics.state==SILPH_FARMER_TRANSFER||s->metrics.state==SILPH_FARMER_BERRY){
        uint32_t phase=s->metrics.state;SilphPromiseMetrics promise={0};status=silph_managed_promise_poll(s->pending,&s->identity,now,&promise);
        if(!status&&promise.state==SILPH_PROMISE_RESOLVED){
            SilphManagedRef result={0};bool fresh=false;status=silph_managed_promise_take(s->pending,&s->identity,now,&result,&fresh);
            if(!status&&s->metrics.state==SILPH_FARMER_CAPTURE)status=capture_response(s,result,fresh,now);
            else if(!status&&s->metrics.state==SILPH_FARMER_TRANSFER){
                SilphReadValue code={0};status=silph_read_field(s->read,result,"Status",SILPH_READ_ENUM32,&code);
                if(!status)transfer_response(s,(uint32_t)code.bits,now);
            }else if(!status&&s->metrics.state==SILPH_FARMER_BERRY){
                SilphReadValue code={0},active={0};status=silph_read_field(s->read,result,"Status",SILPH_READ_ENUM32,&code);
                if(!status)status=silph_read_field(s->read,result,"ActiveItem",SILPH_READ_ENUM32,&active);
                if(!status){if(code.bits==0&&active.bits==s->selected_berry){s->active_item=(unsigned)active.bits;s->metrics.berries++;}
                    else if(code.bits==2&&active.bits)s->active_item=(unsigned)active.bits;else s->berry_suppressed=1;
                    s->metrics.state=code.bits==1?SILPH_FARMER_DONE:SILPH_FARMER_RETRY;}
            }else if(!status){
                if(s->options.use_berries){SilphReadValue active={0};status=silph_read_field(s->read,result,"InitialActiveItem",SILPH_READ_ENUM32,&active);if(!status)s->active_item=(unsigned)active.bits;}
                s->metrics.state=SILPH_FARMER_RETRY;
            }
            int a=release(s,&result),b=silph_managed_promise_close(s->pending);if(!status)status=a?a:b;
        }else if(!status&&promise.state!=SILPH_PROMISE_WAITING)status=terminal_response(s,&promise);
        if(status){s->metrics.promise_phase=phase;s->metrics.promise_state=promise.state;s->metrics.promise_reason=promise.reason;}
    }
    if(!status)status=continue_session(s);
    if(status)s->metrics.error=(uint32_t)status;*output=s->metrics;return status;
}
int silph_farmer_actions_close(void *memory){
    Farmer *s=memory;if(!s||!s->opened)return 0;int a=silph_managed_promise_close(s->pending),b=drop_target(s);
    if(a||b)return a?a:b;int closed=silph_core_close(s->scope);if(closed)return SILPH_MANAGED_STATE;
    free(s->pending);free(s->scope);memset(s,0,sizeof(*s));return 0;
}
