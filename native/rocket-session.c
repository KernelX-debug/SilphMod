#include "rocket-session.h"
#include "rocket-bridge.h"
#include "rocket-policy.h"
#include "rocket-flow.h"
#include "rpc-actions.h"
#include "kecleon-actions.h"
#include "maintenance-actions.h"
#include "response-rewards.h"
#include "managed-promise.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <android/log.h>
#include <math.h>

enum {RS_OPEN=1,RS_UI=2,RS_BATTLE=3,RS_ENCOUNTER=4,RS_CAPTURE=5,RS_HEAL=6,RS_DONE=7,RS_LOSS=8};
typedef struct {
    uint32_t grunt_character;
    void *read,*managed,*game,*link,*rpc,*maintenance,*medicine;SilphScheduleIdentity identity;SilphSessionConfig config;
    SilphRocketMetrics metrics;SilphKecleonEvent event;SilphGameStop stop;SilphManagedRef incident,gui,encounter_state,director,place,encounter,pokemon,attack,map_handler,map_settings,reward_gui;
    RocketDecisionGate gate;RocketBattleClock battle_clock;RocketActionWatch pending_action;uint64_t team[3],sequence,event_sequence,deadline,scan_at,nudge_at,next,attack_until,inventory_at,gui_at,clean_due;
    uint64_t charge_at,director_at,healing_target,healing_until,anchor_until;uintptr_t last_gui;uint32_t opened,rpc_active,rpc_kind,clean_opened,clean_index,clean_batches,dirty,nudged,returning,won,reward_read,berry_attempted,active_item,balls,medicine_active,healing_hp,healing_expected,anchor_expected,charge_slot;
    uint32_t species,iv[3],shiny,preparing,team_ready,team_refreshed,map_disabled,lost,loss_recovery,loss_seen,available_shields,shield_clicked,shield_wait,shield_before;uint64_t map_at,map_requests,capture_finished_at,loss_until,loss_started,loss_clear_at,shield_at;float map_saved[3];char source[97],spawn[17],top_name[128];double latitude,longitude,anchor_latitude,anchor_longitude;
    uint32_t attack_type,last_action_result;uint64_t charge_retry_at,action_log_at,battle_started,accepted_actions,rejected_actions,last_active;
    uint32_t ui_taps,ui_seen,team_choice,team_choice_attempts,result_blocked,headless;uint64_t ui_started,ui_retry_at,result_scan_at,reward_before,transition_since;RocketCaptureGate capture_gate,record_gate;uintptr_t fast_gui;
    SilphManagedRef visual_properties;float visual_saved[4];unsigned visual_attempted,visual_tuned,timing_stage;uint64_t timing_at,timing_request;
} Rocket;
size_t silph_rocket_session_size(void){return sizeof(Rocket);}
static int drop(Rocket *s,SilphManagedRef *ref){return silph_reward_drop(s->managed,ref);}
static int callref(Rocket *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_call(s->read,ref,name,SILPH_READ_REFERENCE,NULL,0,&v);if(!r)*out=v.reference;return r;}
static int fieldref(Rocket *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;return r;}
static int scalar(Rocket *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t *out,bool method){SilphReadValue v={0};int r=method?silph_read_call(s->read,ref,name,kind,NULL,0,&v):silph_read_field(s->read,ref,name,kind,&v);if(!r)*out=v.bits;return r;}
static int typed(Rocket *s,SilphManagedRef ref,const char *space,const char *name,SilphManagedRef *out){return silph_read_field_type(s->read,ref,space,name,out);}
static int text(Rocket *s,SilphManagedRef ref,const char *name,char *out,uint32_t capacity,bool method){SilphManagedRef value={0};int r=method?callref(s,ref,name,&value):fieldref(s,ref,name,&value);uint32_t length=0;if(!r)r=silph_read_utf8(s->read,value,out,capacity,&length);int freed=drop(s,&value);return r?r:freed;}
static bool istype(Rocket *s,SilphManagedRef ref,const char *space,const char *name){return ref.serial&&!silph_read_is_type(s->read,ref,space,name);}
static int action(Rocket *s,const SilphSchedulePermit *p,uint32_t op,SilphManagedRef receiver,const SilphReadArgument *args,uint32_t count){SilphReadValue out={0};int r=silph_rocket_invoke(s->read,p,op,receiver,args,count,&out);int freed=drop(s,&out.reference);return r?r:freed;}
static void phase(Rocket *s,uint32_t value,uint32_t stage,uint64_t deadline){s->metrics.phase=value;s->metrics.stage=stage;s->deadline=deadline;s->dirty=1;}
static int release_candidate(Rocket *s){SilphManagedRef *refs[]={&s->attack,&s->director,&s->place,&s->encounter,&s->pokemon,&s->reward_gui};int result=0;
    silph_rocket_flow_unbind();
    for(unsigned i=0;i<6;i++){int r=drop(s,refs[i]);if(!result)result=r;}memset(&s->gate,0,sizeof(s->gate));memset(&s->battle_clock,0,sizeof(s->battle_clock));memset(&s->pending_action,0,sizeof(s->pending_action));memset(s->team,0,sizeof(s->team));s->preparing=s->team_ready=s->team_refreshed=s->team_choice=s->team_choice_attempts=s->headless=0;return result;}
static void result(Rocket *s,uint32_t reason,bool verified){s->metrics.reason=reason;s->metrics.verified=verified;phase(s,7,RS_DONE,0);}
static int publish_event(Rocket *s){
    SilphKecleonEvent next={.identity=s->identity,.sequence=s->event_sequence+1,.request=s->metrics.request,.phase=s->metrics.phase,
        .move=s->metrics.phase==6?(s->returning?2:1):0,.reason=s->metrics.reason,.verified=s->metrics.verified,.latitude=s->latitude,.longitude=s->longitude};
    if(next.phase!=1&&next.phase!=8){memcpy(next.source,s->source,97);memcpy(next.fort,s->stop.fort,161);memcpy(next.incident,s->stop.incident,161);}
    if(!s->dirty)return 0;int r=silph_session_link_event(s->link,&next);if(r==SILPH_SCHEDULE_BUSY)return 0;
    if(!r){s->event=next;s->event_sequence++;s->dirty=0;}return r;
}
int silph_rocket_session_publish(void *memory){Rocket *s=memory;return s&&s->opened?publish_event(s):SILPH_MANAGED_STATE;}
int silph_rocket_session_open(void *memory,size_t bytes,void *read,void *managed,void *game,void *link,const SilphScheduleIdentity *id,const SilphSessionConfig *config){
    if(!memory||bytes!=sizeof(Rocket)||!read||!managed||!game||!link||!id||!config||config->mode!=3)return SILPH_MANAGED_ARGUMENT;
    Rocket *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->link=link;s->identity=*id;s->config=*config;s->opened=1;
    s->rpc=calloc(1,silph_rpc_action_size());s->maintenance=calloc(1,silph_maintenance_size());s->medicine=calloc(1,silph_managed_promise_size());if(!s->rpc||!s->maintenance||!s->medicine)return SILPH_MANAGED_CAPACITY;
    int r=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),read,managed,game,id);
    if(!r)r=silph_game_read_rocket_service(game,0,&s->incident);if(!r)r=silph_game_read_rocket_service(game,1,&s->gui);
    s->metrics.request=1;phase(s,1,0,0);return r;
}
bool silph_rocket_inventory_due(void *memory){Rocket *s=memory;if(!s||!s->opened)return true;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);uint64_t n=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;
    if(n<s->inventory_at)return false;s->inventory_at=n+UINT64_C(2000000000);return true;}
static int command(Rocket *s,const SilphKecleonCommand *c,uint64_t now){
    if(memcmp(&c->identity,&s->identity,sizeof(s->identity)))return SILPH_MANAGED_STALE;if(c->request!=s->metrics.request)return 0;
    if(c->kind==1&&s->metrics.phase==6&&s->returning&&!strcmp(c->source,"anchor")){
        s->anchor_latitude=c->latitude;s->anchor_longitude=c->longitude;s->anchor_expected=1;
    }else if(c->kind==1&&s->metrics.phase==1){
        int r=release_candidate(s);if(r)return r;SilphManagedRef cached={0};s->reward_before=0;r=typed(s,s->incident,"Holoholo.Rpc","InvasionEncounterOutProto",&cached);
        if(!r&&cached.serial)r=scalar(s,cached,"EncounterId",SILPH_READ_U64,&s->reward_before,false);int freed=drop(s,&cached);if(r||freed)return r?r:freed;
        s->grunt_character=0;s->latitude=c->latitude;s->longitude=c->longitude;memcpy(s->source,c->source,97);memset(&s->stop,0,sizeof(s->stop));
        s->metrics.reason=s->metrics.verified=s->metrics.result=0;s->won=s->reward_read=s->nudged=s->returning=s->berry_attempted=s->active_item=s->balls=s->anchor_expected=s->healing_expected=s->lost=s->loss_recovery=s->loss_seen=s->shield_clicked=s->shield_wait=s->available_shields=0;s->last_gui=s->capture_finished_at=s->loss_clear_at=0;s->charge_slot=2;
        s->charge_retry_at=s->action_log_at=s->battle_started=s->accepted_actions=s->rejected_actions=s->last_active=0;s->last_action_result=0;
        phase(s,2,0,now+UINT64_C(15000000000));
    }else if(c->kind==2&&(s->metrics.phase==2||s->metrics.phase==6)&&c->move==(s->metrics.phase==2?0:s->returning?2:1)){
        if(!c->accepted){result(s,2,false);return 0;}
        if(s->returning){s->returning=0;s->next=now+UINT64_C(100000000);s->anchor_until=now+UINT64_C(10000000000);phase(s,5,RS_CAPTURE,now+UINT64_C(120000000000));}
        else{s->scan_at=now+UINT64_C(250000000);s->nudge_at=now+UINT64_C(4000000000);phase(s,3,0,now+UINT64_C(18000000000));}
    }else if(c->kind==3&&s->metrics.phase==4){if(c->accepted){int r=silph_rocket_flow_bind(s->managed,s->incident,s->stop.incident,s->metrics.request,s->grunt_character);if(r)return r;s->ui_taps=s->ui_seen=0;s->fast_gui=0;s->ui_started=now;s->ui_retry_at=now+UINT64_C(100000000);phase(s,5,RS_OPEN,now+UINT64_C(240000000000));}else result(s,4,true);
    }else if(c->kind==4&&s->metrics.phase==7){
        int r=release_candidate(s);if(r)return r;
        if(s->loss_recovery){s->metrics.reason=7;phase(s,8,0,0);}
        else if(s->metrics.rockets>=s->config.max_encounters){s->metrics.reason=5;phase(s,8,0,0);}
        else{s->metrics.request++;s->metrics.reason=0;memset(s->source,0,sizeof(s->source));phase(s,1,0,0);}
    }else if(c->kind==5&&s->metrics.phase==1){s->metrics.reason=6;phase(s,8,0,0);}return 0;
}
static int stop_at(Rocket *s,uint64_t now,uint64_t wall){
    if(now<s->scan_at)return 0;s->scan_at=now+UINT64_C(500000000);SilphGameStop found={0};
    int r=silph_game_read_stop_at(s->game,50,s->latitude,s->longitude,5,&found);
    if(r==SILPH_MANAGED_NOT_READY){
        if(now>=s->deadline){result(s,3,false);return 0;}
        if(!s->nudged&&now>=s->nudge_at){s->nudged=1;phase(s,6,0,now+UINT64_C(15000000000));}return 0;
    }
    if(r)return r;
    if(found.incident_type!=1||!found.active||found.complete){result(s,1,true);return 0;}
    r=silph_game_read_place(s->game,found.fort,&s->place);if(r)return r;
    uint64_t character=0;SilphManagedRef display={0};r=scalar(s,s->place,"get_NPCCharacter",SILPH_READ_ENUM32,&character,true);
    if(!r&&!((character>=4&&character<=39)||(character>=47&&character<=50))){drop(s,&s->place);result(s,1,true);return 0;}
    if(!r)r=text(s,s->place,"get_CurrentIncidentId",found.incident,sizeof(found.incident),true);
    if(!r)r=typed(s,s->place,"Holoholo.Rpc","PokestopIncidentDisplayProto",&display);
    if(!r)r=scalar(s,display,"IncidentExpirationMs",SILPH_READ_I64,&found.expires_ms,false);
    drop(s,&display);if(r)return r;
    if(!found.incident[0]||found.expires_ms<=wall+10000){drop(s,&s->place);result(s,1,true);return 0;}
    s->grunt_character=(uint32_t)character;s->stop=found;phase(s,4,0,now+UINT64_C(10000000000));return 0;
}
static void refresh_map(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    if(s->map_disabled||now<s->map_at)return;s->map_at=now+UINT64_C(8000000000);int r=0;
    if(!s->map_handler.serial)r=silph_game_read_rocket_service(s->game,5,&s->map_handler);
    if(!r&&!s->map_settings.serial){
        SilphManagedRef service={0},global={0};r=typed(s,s->map_handler,"Niantic.Holoholo","IGlobalSettings",&service);
        if(!r)r=callref(s,service,"get_Settings",&global);if(!r)r=fieldref(s,global,"MapSettings",&s->map_settings);
        if(!r)r=silph_rocket_map_settings(s->read,permit,s->map_settings,s->map_saved,false);drop(s,&service);drop(s,&global);
        if(r)drop(s,&s->map_settings);
    }
    if(!r)r=action(s,permit,RK_MAP_REFRESH,s->map_handler,NULL,0);
    if(r){s->map_disabled=1;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","map refresh unavailable error=%d",r);}
    else __android_log_print(ANDROID_LOG_INFO,"SilphRocket","map refresh requested sequence=%llu",(unsigned long long)++s->map_requests);
}
static int care(Rocket *,const SilphSchedulePermit *,uint64_t);
static int open_encounter(Rocket *,uint64_t);
static int prepare_team(Rocket *s,const SilphSchedulePermit *permit,SilphManagedRef party,uint64_t now){
    if(s->lost){s->gui_at=now+UINT64_C(500000000);int r=action(s,permit,RK_PARTY_CANCEL,party,NULL,0);if(!r)s->loss_seen=1;return r;}
    if((s->config.flags&SILPH_CFG_RECOMMENDED_TEAM)&&!s->team_choice){
        s->gui_at=now+UINT64_C(150000000);
        if(s->team_choice_attempts++>=20){s->metrics.error=121;phase(s,8,0,0);return 0;}
        int r=action(s,permit,RK_PARTY_RECOMMEND,party,NULL,0);if(r==SILPH_MANAGED_NOT_READY)return 0;
        if(!r){s->team_choice++;s->team_choice_attempts=0;}return r;
    }
    uint64_t team[3]={0};bool usable=false;int r=silph_rocket_prepared_team(s->read,party,team,&usable);
    if(r==SILPH_MANAGED_NOT_READY)return 0;
    if(!r){
        if(!s->team[0])memcpy(s->team,team,sizeof(team));else if(memcmp(s->team,team,sizeof(team)))r=SILPH_MANAGED_STALE;
        if(!r&&!s->team_ready){s->preparing=1;r=care(s,permit,now);}
        if(!r&&s->team_ready){
            s->gui_at=now+UINT64_C(100000000);
            if(usable&&s->team_refreshed){s->gui_at=now+UINT64_C(2000000000);r=action(s,permit,RK_SUBMIT,party,NULL,0);}
            else if(s->team_refreshed++<3){SilphManagedRef bag={0};r=silph_game_read_rocket_service(s->game,7,&bag);SilphReadArgument argument={.kind=SILPH_READ_REFERENCE,.value.reference=bag};if(!r)r=action(s,permit,RK_PARTY_REFRESH,party,&argument,1);drop(s,&bag);}
            else{s->metrics.error=117;phase(s,8,0,0);}
        }
    }
    return r;
}
static int ui(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    SilphReadValue top={0};int r=silph_rocket_invoke(s->read,permit,RK_TOP,s->gui,NULL,0,&top);if(r)return r;
    if(!top.reference.serial)return 0;void *object=NULL;r=silph_managed_target(s->managed,top.reference,&object);
    char name[128]={0};if(!silph_rocket_class(s->read,top.reference,name,sizeof(name))&&strcmp(name,s->top_name)){
        memcpy(s->top_name,name,strlen(name)+1);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","gui=%s request=%llu",name,(unsigned long long)s->metrics.request);
    }
    bool fast=istype(s,top.reference,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")||istype(s,top.reference,"Niantic.Holoholo.Combat.NpcTrainers","NpcTrainerInterstitialGui")||istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentInvasionDialogGui")||istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentCombatResolveGuiController");
    if(!r&&fast&&s->fast_gui!=(uintptr_t)object){int tuned=action(s,permit,RK_FAST_GUI,top.reference,NULL,0);s->fast_gui=(uintptr_t)object;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","presentation shortened gui=%s result=%d",name,tuned);}
    uint32_t op=0;
    if(s->metrics.stage==RS_UI&&(istype(s,top.reference,"Niantic.Holoholo.Combat.NpcTrainers","NpcTrainerInterstitialGui")||istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentInvasionDialogGui")||istype(s,top.reference,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")||istype(s,top.reference,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget")))s->ui_seen=1;
    if(istype(s,top.reference,"Niantic.Holoholo.Combat.NpcTrainers","NpcTrainerInterstitialGui"))op=s->lost?RK_CANCEL:silph_rocket_flow_challenged(s->managed,top.reference)?0:RK_CHALLENGE;
    else if(istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentInvasionDialogGui"))op=RK_DIALOG;
    else if(s->metrics.stage==RS_UI&&istype(s,top.reference,"Niantic.Holoholo.Pokestop","PokestopSpinnerInteractionGuiController"))op=RK_STOP_EXIT;
    else if(s->lost&&istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentCombatResolveGuiController"))op=RK_INCIDENT_CLOSE;
    else if(s->lost&&istype(s,top.reference,"Niantic.Holoholo.Combat.Resolution","CombatResolveGuiController"))op=RK_RESULT_CLOSE;
    else if(s->won&&istype(s,top.reference,"Niantic.Holoholo.Incident","IncidentCombatResolveGuiController")){
        if(!s->reward_gui.serial)r=silph_managed_retain(s->managed,object,false,&s->reward_gui);
        if(!s->reward_read){SilphManagedRef info={0};int p=typed(s,top.reference,"Niantic.Holoholo.Incident","IIncidentCombatResolveInformation",&info);
            uint64_t xp=0,dust=0;if(!p)p=silph_reward_loot(s->read,s->managed,info,"<ItemRewards>k__BackingField",&xp,&dust);if(p)s->metrics.reward_error=(uint32_t)p;else{s->metrics.xp+=xp;s->metrics.stardust+=dust;}
            drop(s,&info);s->reward_read=1;}
        if(!r)r=open_encounter(s,now);
    }else if(istype(s,top.reference,"Niantic.Holoholo.UI","ItemFragmentAssemblyModalGuiController")){
        SilphManagedRef service={0};r=silph_game_read_rocket_service(s->game,3,&service);if(!r)r=action(s,permit,RK_GEM,service,NULL,0);drop(s,&service);
    }
    if(!r&&op&&now>=s->gui_at&&((uintptr_t)object!=s->last_gui||op==RK_DIALOG||now-s->gui_at>UINT64_C(2000000000))){
        s->last_gui=(uintptr_t)object;s->gui_at=now+UINT64_C(50000000);r=action(s,permit,op,top.reference,NULL,0);
        if(!r&&s->lost)s->loss_seen=1;
    }
    if(!s->director.serial&&!typed(s,top.reference,"Niantic.Holoholo.VNext","IBattleDirector",&s->director)){
        if((s->config.flags&SILPH_CFG_TRAINER_NAME)&&istype(s,top.reference,"Niantic.Holoholo.Combat.UI","CombatVNextGuiController"))action(s,permit,RK_PLAYER_ALIAS,top.reference,NULL,0);
        __android_log_print(ANDROID_LOG_INFO,"SilphRocket","battle director resolved request=%llu",(unsigned long long)s->metrics.request);
    }
    if(!r&&(s->metrics.stage==RS_UI||s->lost)&&istype(s,top.reference,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")&&now>=s->gui_at){
        SilphReadValue party={0};r=silph_rocket_invoke(s->read,permit,RK_PARTY,top.reference,NULL,0,&party);
        if(!r&&party.reference.serial)r=prepare_team(s,permit,party.reference,now);
        drop(s,&party.reference);
    }
    else if(!r&&(s->metrics.stage==RS_UI||s->lost)&&istype(s,top.reference,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget")&&now>=s->gui_at)r=prepare_team(s,permit,top.reference,now);
    if(!r&&!s->lost&&s->available_shields&&(!s->shield_clicked||(s->shield_wait&&now>=s->shield_at))&&istype(s,top.reference,"Niantic.Holoholo.Combat.InputChallenge","ShieldModalGuiController")){
        r=action(s,permit,RK_SHIELD,top.reference,NULL,0);
        if(r==SILPH_MANAGED_NOT_READY)r=0;
        else if(!r){s->shield_clicked=s->shield_wait=1;s->shield_before=s->available_shields;s->shield_at=now+UINT64_C(1000000000);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","shield requested remaining=%u",s->shield_before);}
    }
    int freed=drop(s,&top.reference);return r?r:freed;
}
static int scan_director(Rocket *s,const SilphSchedulePermit *permit,SilphManagedRef set){
    if(!set.serial)return 0;SilphManagedRef iterator={0},current={0};SilphReadValue value={0};
    int r=silph_read_call(s->read,set,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&value);
    if(r==SILPH_MANAGED_CONTRACT)r=silph_read_call(s->read,set,"GetEnumerator",SILPH_READ_REFERENCE,NULL,0,&value);iterator=value.reference;
    for(unsigned i=0;!r&&i<128;i++){
        uint64_t more=0;r=scalar(s,iterator,"MoveNext",SILPH_READ_BOOL,&more,true);if(r||!more)break;
        r=callref(s,iterator,"get_Current",&current);
        if(!r&&istype(s,current,"Niantic.Holoholo.Combat.UI","CombatVNextGuiController")){if(s->config.flags&SILPH_CFG_TRAINER_NAME){int alias=action(s,permit,RK_PLAYER_ALIAS,current,NULL,0);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","battle player label result=%d",alias);}r=typed(s,current,"Niantic.Holoholo.VNext","IBattleDirector",&s->director);}
        drop(s,&current);if(s->director.serial)break;
    }
    SilphManagedRef *refs[]={&current,&iterator};for(unsigned i=0;i<2;i++){int d=drop(s,refs[i]);if(!r)r=d;}return r;
}
static int find_director(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    if(s->director.serial||now<s->director_at)return 0;s->director_at=now+UINT64_C(1000000000);
    SilphManagedRef set={0};int r=fieldref(s,s->gui,"dddt",&set);if(!r)r=scan_director(s,permit,set);drop(s,&set);
    const uint32_t layers[]={300,200,320,100,0,350,400,500,600,700,800,900,1000,1100};
    for(unsigned i=0;!r&&!s->director.serial&&i<14;i++){
        SilphReadArgument argument={.kind=SILPH_READ_ENUM32,.value.bits=layers[i]};SilphReadValue value={0};
        r=silph_rocket_invoke(s->read,permit,RK_LAYER,s->gui,&argument,1,&value);if(!r)r=scan_director(s,permit,value.reference);drop(s,&value.reference);
    }
    if(!r&&s->director.serial)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","active battle director resolved request=%llu",(unsigned long long)s->metrics.request);return r;
}
static int health_from_state(Rocket *s,SilphManagedRef state,RocketHealth *health){SilphManagedRef pokemon={0};uint64_t id=0,hp=0,max=0;int r=callref(s,state,"get_Pokemon",&pokemon);
    if(!r)r=scalar(s,pokemon,"Id",SILPH_READ_U64,&id,false);if(!r)r=scalar(s,state,"CurrentHealth",SILPH_READ_I32,&hp,false);if(!r)r=scalar(s,state,"MaxHealth",SILPH_READ_I32,&max,false);
    if(!r&&(hp>max||!max||max>100000||!id))r=SILPH_MANAGED_CONTRACT;
    if(!r)*health=(RocketHealth){.id=id,.hp=(int32_t)hp,.max_hp=(int32_t)max,.known=true};drop(s,&pokemon);return r;
}
static int battle_view(Rocket *s,RocketBattleView *out,uint32_t *status){
    *out=(RocketBattleView){.bound=true,.alive=true,.merged=true,.started=true,.generation=s->metrics.request,.charge_slot=2};
    SilphManagedRef state={0},actor={0},sync={0},active={0},roster={0},values={0},iterator={0},current={0},pokemon={0},moves={0};uint64_t n=0;
    int r=callref(s,s->director,"get_CurrentState",&state);if(!r&&!state.serial)r=SILPH_MANAGED_NOT_READY;
    if(!r)r=scalar(s,state,"BattleStatus",SILPH_READ_ENUM32,&n,false);*status=(uint32_t)n;
    if(!r&&*status!=1){drop(s,&state);return 0;}
    if(!r)r=callref(s,state,"get_LocalActor",&actor);if(!r)r=callref(s,state,"get_CurrentTurnSyncInfo",&sync);
    if(!r)r=scalar(s,sync,"get_SyncedTurnValid",SILPH_READ_BOOL,&n,true);if(!r&&!n)r=SILPH_MANAGED_NOT_READY;
    if(!r)r=scalar(s,sync,"get_SyncedTurn",SILPH_READ_I64,&out->turn,true);if(!r&&out->turn>INT64_MAX)r=SILPH_MANAGED_NOT_READY;out->serial=out->turn;
    if(!r)r=scalar(s,state,"get_UIMode",SILPH_READ_ENUM32,&n,true);out->ui_mode=(uint32_t)n;
    if(!r)r=scalar(s,s->director,"get_InputBlocked",SILPH_READ_BOOL,&n,true);out->input_blocked=n&&out->ui_mode!=5&&out->ui_mode!=6;
    if(!r)r=callref(s,actor,"get_ActivePokemonState",&active);
    if(!r&&!active.serial)r=SILPH_MANAGED_NOT_READY;
    if(!r){RocketHealth h={0};r=health_from_state(s,active,&h);out->active=h.id;}
    if(!r)r=scalar(s,active,"CurrentEnergy",SILPH_READ_I32,&n,false);out->energy=(int32_t)n;
    if(!r)r=fieldref(s,actor,"Roster",&roster);if(!r)r=callref(s,roster,"get_Values",&values);
    SilphReadValue v={0};if(!r)r=silph_read_call(s->read,values,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&v);iterator=v.reference;
    unsigned count=0;
    for(;!r&&count<3;count++){
        r=scalar(s,iterator,"MoveNext",SILPH_READ_BOOL,&n,true);if(r||!n)break;r=callref(s,iterator,"get_Current",&current);
        if(!r)r=health_from_state(s,current,&out->health[count]);out->team[count]=out->health[count].id;drop(s,&current);
    }
    if(!r&&count!=3)r=SILPH_MANAGED_NOT_READY;
    if(!r&&s->team[0]){for(unsigned i=0;i<3;i++){bool found=false;for(unsigned j=0;j<3;j++)if(s->team[i]==out->team[j])found=true;if(!found)r=SILPH_MANAGED_STALE;}}
    if(!r&&!s->team[0])memcpy(s->team,out->team,sizeof(s->team));
    if(!r)r=scalar(s,actor,"get_RemainingCombatShields",SILPH_READ_I32,&n,true);out->shields=(uint32_t)n;
    SilphReadArgument index={.kind=SILPH_READ_I32,.value.bits=3};v=(SilphReadValue){0};
    if(!r)r=callref(s,active,"get_Pokemon",&pokemon);if(!r)r=scalar(s,pokemon,"PokedexId",SILPH_READ_ENUM32,&n,false);out->species=(uint32_t)n;if(!r)r=fieldref(s,pokemon,"Moves",&moves);
    for(unsigned i=0;!r&&i<3;i++){
        index.value.bits=i+1;v=(SilphReadValue){0};int move=silph_read_call(s->read,moves,"get_Item",SILPH_READ_REFERENCE,&index,1,&v);
        if(move){if(i==2)continue;r=move;break;}
        out->moves[i].slot=(int32_t)i+1;
        r=scalar(s,v.reference,"Move",SILPH_READ_ENUM32,&n,false);out->moves[i].move=(int32_t)n;
        if(!r)r=scalar(s,v.reference,"EnergyDelta",SILPH_READ_I32,&n,false);out->moves[i].energy_delta=(int32_t)n;
        if(!r)r=scalar(s,v.reference,"DurationTurns",SILPH_READ_I32,&n,false);out->moves[i].duration=(int32_t)n;drop(s,&v.reference);
    }
    SilphManagedRef *refs[]={&current,&moves,&pokemon,&iterator,&values,&roster,&active,&sync,&actor,&state};
    for(unsigned i=0;i<10;i++){int d=drop(s,refs[i]);if(!r)r=d;}return r;
}
static int poll_attack(Rocket *s,const RocketBattleView *view,uint64_t now,bool *ready){
    *ready=false;
    if(s->attack.serial){uint64_t complete=0,error=0;int r=scalar(s,s->attack,"completeCalled",SILPH_READ_BOOL,&complete,false);if(!r)r=scalar(s,s->attack,"errorCalled",SILPH_READ_BOOL,&error,false);
        if(r)return r;
        if(!complete&&!error){
            if(!rocket_action_observed(&s->pending_action,view,now)){
                if(now>s->attack_until){
                    __android_log_print(ANDROID_LOG_ERROR,"SilphRocket","battle action timed out type=%u ui=%u/%u turn=%llu/%llu activeChanged=%u blocked=%u request=%llu",s->attack_type,s->pending_action.ui_mode,view->ui_mode,(unsigned long long)s->pending_action.turn,(unsigned long long)view->turn,s->pending_action.active!=view->active,view->input_blocked,(unsigned long long)s->metrics.request);
                    s->metrics.error=113;phase(s,8,0,0);
                }return 0;
            }
            __android_log_print(ANDROID_LOG_INFO,"SilphRocket","pending action retired after battle progress type=%u ui=%u/%u turn=%llu/%llu",s->attack_type,s->pending_action.ui_mode,view->ui_mode,(unsigned long long)s->pending_action.turn,(unsigned long long)view->turn);
        }else{
            uint64_t response=0;if(!error)r=scalar(s,s->attack,"completedValue",SILPH_READ_ENUM32,&response,false);if(r)return r;
            if(!error&&response==1)s->accepted_actions++;else if(response!=6){s->rejected_actions++;
                if((s->attack_type==2||s->attack_type==3)&&view->ui_mode==0)s->charge_retry_at=now+UINT64_C(1000000000);
                if(now>=s->action_log_at||response!=s->last_action_result){__android_log_print(ANDROID_LOG_INFO,"SilphRocket","action settled type=%u ui=%u result=%llu error=%llu",s->attack_type,view->ui_mode,(unsigned long long)response,(unsigned long long)error);s->action_log_at=now+UINT64_C(5000000000);s->last_action_result=(uint32_t)response;}
            }
        }
        r=drop(s,&s->attack);if(r)return r;memset(&s->pending_action,0,sizeof(s->pending_action));
        if(s->gate.pending&&!rocket_battle_settled(&s->gate,view))return 0;
    }else if(s->gate.pending&&!rocket_battle_settled(&s->gate,view))return 0;
    *ready=true;return 0;
}
static int battle(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    RocketBattleView view;uint32_t state=0;int r=battle_view(s,&view,&state);if(r==SILPH_MANAGED_NOT_READY)return 0;if(r)return r;
    if(state==2||state==7){if(!s->won){s->won=1;s->metrics.rockets++;s->metrics.stage=RS_ENCOUNTER;drop(s,&s->attack);s->gate.pending=false;s->next=now+UINT64_C(300000000);}
        __android_log_print(ANDROID_LOG_INFO,"SilphRocket","battle completed status=%u ms=%llu accepted=%llu rejected=%llu",state,(unsigned long long)((now-s->battle_started)/1000000),(unsigned long long)s->accepted_actions,(unsigned long long)s->rejected_actions);
        return 0;}
    if(state>=3&&state!=7){s->metrics.result=state;s->lost=1;s->metrics.stage=RS_LOSS;s->loss_started=now;s->loss_until=now+UINT64_C(20000000000);drop(s,&s->attack);s->gate.pending=false;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","battle ended without victory status=%u",state);return 0;}
    if(state!=1)return 0;
    if(!rocket_battle_observe(&s->battle_clock,&view))return 0;
    if(!s->battle_started)s->battle_started=now;
    if(s->last_active!=view.active){s->last_active=view.active;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","active species=%u fast=%d rawTurns=%d energyDelta=%d charge1=%d charge2=%d energy=%d",view.species,view.moves[0].move,view.moves[0].duration,view.moves[0].energy_delta,view.moves[1].move,view.moves[2].move,view.energy);}
    s->available_shields=view.shields;
    if(s->shield_wait&&view.shields<s->shield_before){s->shield_wait=0;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","shield confirmed remaining=%u",view.shields);}
    if(view.ui_mode!=6){s->shield_clicked=0;s->shield_wait=0;}
    view.shields=0;
    bool ready=false;r=poll_attack(s,&view,now,&ready);if(r||!ready)return r;
    view.charge_slot=s->charge_slot;
    if(now<s->charge_retry_at&&view.ui_mode==0)for(unsigned i=0;i<3;i++)if(view.moves[i].slot==2||view.moves[i].slot==3)view.moves[i].disabled=true;
    if(view.ui_mode==5){view.progress_ready=now>=s->charge_at;view.progress=1.0f;}else s->charge_at=0;
    RocketDecision decision={0};uint32_t kind=rocket_battle_decide(&view,&s->gate,&decision);
    if(kind==ROCKET_INVALID)return SILPH_MANAGED_CONTRACT;
    if(kind!=ROCKET_WAIT){r=silph_rocket_attack(s->read,permit,s->director,decision.attack_type?decision.attack_type:7,decision.score,decision.outgoing,decision.incoming,&s->attack);
        if(!r){s->attack_type=decision.attack_type?decision.attack_type:7;if(kind==ROCKET_CHARGED&&view.ui_mode==0)s->charge_slot=decision.attack_type;s->attack_until=now+UINT64_C(18000000000);s->charge_at=now+UINT64_C(1000000000);
            s->pending_action=(RocketActionWatch){.generation=view.generation,.turn=view.turn,.active=view.active,.incoming=decision.incoming,.until=s->attack_until,.type=s->attack_type,.ui_mode=view.ui_mode,.energy=view.energy};}}
    return r;
}
static int open_encounter(Rocket *s,uint64_t now){
    if(s->headless||!s->won||!s->reward_gui.serial)return 0;
    char incident[161]={0};int current=text(s,s->incident,"get_CurrentIncidentId",incident,sizeof(incident),true);
    if(current||strcmp(incident,s->stop.incident))return 0;
    int r=typed(s,s->incident,"Holoholo.Rpc","InvasionEncounterOutProto",&s->encounter);if(r||!s->encounter.serial)return r;
    uint64_t id=0,n=0;r=scalar(s,s->encounter,"Status",SILPH_READ_ENUM32,&n,false);
    if(!r&&n!=1){drop(s,&s->encounter);return 0;}
    if(!r)r=scalar(s,s->encounter,"EncounterId",SILPH_READ_U64,&id,false);
    if(!r&&(!id||id==s->metrics.encounter||id==s->reward_before)){drop(s,&s->encounter);return 0;}
    if(!r)r=fieldref(s,s->encounter,"EncounterPokemon",&s->pokemon);
    const char *names[]={"individualAttack_","individualDefense_","individualStamina_"};
    for(unsigned i=0;!r&&i<3;i++){r=scalar(s,s->pokemon,names[i],SILPH_READ_I32,&n,false);s->iv[i]=(uint32_t)n;if(!r&&n>15)r=SILPH_MANAGED_CONTRACT;}
    if(!r)r=scalar(s,s->pokemon,"pokemonId_",SILPH_READ_I32,&n,false);s->species=(uint32_t)n;
    SilphManagedRef display={0};if(!r)r=fieldref(s,s->pokemon,"pokemonDisplay_",&display);if(!r)r=scalar(s,display,"Shiny",SILPH_READ_BOOL,&n,false);s->shiny=(uint32_t)n;drop(s,&display);
    if(!r)r=text(s,s->encounter,"SpawnPointGuid",s->spawn,sizeof(s->spawn),false);
    if(!r)r=scalar(s,s->encounter,"ThrowsRemaining",SILPH_READ_I32,&n,false);s->balls=(uint32_t)n;
    if(!r){s->headless=1;s->metrics.encounter=id;s->metrics.encounters++;s->metrics.stage=RS_CAPTURE;
        __android_log_print(ANDROID_LOG_INFO,"SilphRocket","native shadow response ready encounter=%llu; camera UI not opened",(unsigned long long)id);
        __android_log_print(ANDROID_LOG_INFO,"SilphRocket","shadow encounter species=%u iv=%u/%u/%u shiny=%u accepted=%u",s->species,s->iv[0],s->iv[1],s->iv[2],s->shiny,rocket_catch_accept(s->config.filters,s->iv[0],s->iv[1],s->iv[2],s->shiny));
        if(rocket_catch_accept(s->config.filters,s->iv[0],s->iv[1],s->iv[2],s->shiny)){s->returning=1;phase(s,6,RS_CAPTURE,now+UINT64_C(15000000000));}
    }
    return r;
}
static int rpc_send(Rocket *s,const SilphSchedulePermit *permit,uint32_t kind,const SilphRequestInput *input,uint64_t now){
    int r=silph_rpc_action_send(s->rpc,permit,kind,input,++s->sequence,now);if(!r){s->rpc_active=1;s->rpc_kind=kind;}return r;
}
static int finish_encounter(Rocket *s,const SilphSchedulePermit *permit,uint32_t reason){
    if(s->headless){
        char incident[161]={0};int r=text(s,s->incident,"get_CurrentIncidentId",incident,sizeof(incident),true);
        if(!r&&strcmp(incident,s->stop.incident))r=SILPH_MANAGED_STALE;
        SilphReadArgument args[]={{.kind=SILPH_READ_U64,.value.bits=s->metrics.encounter},{.kind=SILPH_READ_ENUM32,.value.bits=reason}};
        if(!r)r=action(s,permit,RK_REWARD_COMPLETE,s->incident,args,2);
        if(!r&&s->reward_gui.serial)r=action(s,permit,RK_REWARD_DISMISS,s->reward_gui,NULL,0);
        if(!r){s->headless=0;s->metrics.stage=RS_HEAL;s->next=0;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","native shadow result completed reason=%u",reason);}return r;
    }
    SilphReadArgument arg={.kind=SILPH_READ_ENUM32,.value.bits=reason};int r=action(s,permit,RK_ENCOUNTER_FINISH,s->encounter_state,&arg,1);
    s->metrics.stage=RS_HEAL;s->next=0;return r;
}
static int capture(Rocket *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now){
    if(s->metrics.phase!=5||s->returning||s->rpc_active||now<s->next)return 0;
    if(!rocket_catch_accept(s->config.filters,s->iv[0],s->iv[1],s->iv[2],s->shiny))return finish_encounter(s,permit,1);
    if(s->anchor_expected){
        double lat=0,lon=0;int r=silph_game_read_rocket_position(s->game,&lat,&lon);if(r&&r!=SILPH_MANAGED_NOT_READY)return r;
        double dy=(lat-s->anchor_latitude)*111195,dx=remainder(lon-s->anchor_longitude,360.0)*111195*cos(s->anchor_latitude*0.017453292519943295);
        if(r||hypot(dx,dy)>12){if(now>=s->anchor_until){s->metrics.error=118;phase(s,8,0,0);}return 0;}
        s->anchor_expected=0;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","capture location confirmed distance=%.2f",hypot(dx,dy));
    }
    if(snapshot->pokemon_count>=snapshot->max_pokemon){s->metrics.reason=8;phase(s,8,0,0);return 0;}
    if(!s->balls)return finish_encounter(s,permit,2);
    SilphRequestInput input={.abi=1,.encounter=s->metrics.encounter,.quality=s->config.quality,.curve=!!(s->config.flags&SILPH_CFG_CURVE),.item=5};memcpy(input.location,s->spawn,17);
    if((s->config.flags&SILPH_CFG_BERRIES)&&!s->active_item&&!s->berry_attempted){
        s->berry_attempted=1;for(unsigned i=0;i<s->config.berry_count;i++){uint32_t count=0;int r=silph_game_read_item(s->game,(int32_t)s->config.berries[i],&count);if(r)return r;
            if(count){input.item=(int32_t)s->config.berries[i];return rpc_send(s,permit,SILPH_RPC_KECLEON_BERRY,&input,now);}}
    }
    int r=rpc_send(s,permit,SILPH_RPC_ROCKET_CAPTURE,&input,now);if(!r){s->metrics.throws++;s->balls--;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","shadow throw sent request=%llu returnCompleted=%u",(unsigned long long)s->metrics.request,!s->returning&&!s->anchor_expected);}return r;
}
static int care(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    if(s->rpc_active||now<s->next)return 0;
    if(s->capture_finished_at&&now<s->capture_finished_at+UINT64_C(500000000))return 0;
    if(!s->team[0]){result(s,s->metrics.encounter?0:7,s->metrics.encounter!=0);return 0;}
    if(s->medicine_active){
        SilphPromiseMetrics metrics={0};int r=silph_managed_promise_poll(s->medicine,&s->identity,now,&metrics);if(r||metrics.state==SILPH_PROMISE_WAITING)return r;
        if(metrics.state!=SILPH_PROMISE_RESOLVED){s->metrics.error=116;phase(s,8,0,0);return 0;}
        SilphManagedRef reply={0};bool fresh=false;uint64_t id=0,hp=0,max=0;
        r=silph_managed_promise_take(s->medicine,&s->identity,now,&reply,&fresh);if(!r&&!fresh)r=SILPH_MANAGED_STALE;
        if(!r)r=scalar(s,reply,"id_",SILPH_READ_U64,&id,false);if(!r)r=scalar(s,reply,"stamina_",SILPH_READ_I32,&hp,false);if(!r)r=scalar(s,reply,"maxStamina_",SILPH_READ_I32,&max,false);
        if(!r&&(id!=s->healing_target||hp<=s->healing_hp||hp>max))r=SILPH_MANAGED_CONTRACT;
        if(!r){s->healing_expected=(uint32_t)hp;s->healing_until=now+UINT64_C(8000000000);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","team medicine confirmed hp=%llu/%llu",(unsigned long long)hp,(unsigned long long)max);}
        drop(s,&reply);int d=silph_managed_promise_close(s->medicine);s->medicine_active=0;s->next=now+UINT64_C(100000000);return r?r:d;
    }
    RocketHealth health[3]={0};int r=0;
    for(unsigned i=0;!r&&i<3;i++){SilphManagedRef pokemon={0};uint64_t hp=0,max=0;r=silph_game_read_owned(s->game,s->team[i],&pokemon);
        if(!r)r=scalar(s,pokemon,"stamina_",SILPH_READ_I32,&hp,false);if(!r)r=scalar(s,pokemon,"maxStamina_",SILPH_READ_I32,&max,false);
        if(!r)health[i]=(RocketHealth){s->team[i],(int32_t)hp,(int32_t)max,true};drop(s,&pokemon);
    }
    if(!r&&s->healing_expected){for(unsigned i=0;i<3;i++)if(health[i].id==s->healing_target){
        if(health[i].hp<(int32_t)s->healing_expected){if(now>=s->healing_until){s->metrics.error=119;phase(s,8,0,0);}return 0;}s->healing_expected=0;
    }}
    RocketCareItems items={0};int32_t *counts[]={&items.potion,&items.super_potion,&items.hyper_potion,&items.max_potion,&items.revive,&items.max_revive};
    const int ids[]={101,102,103,104,201,202};
    for(unsigned i=0;!r&&i<6;i++){uint32_t count=0;r=silph_game_read_item(s->game,ids[i],&count);*counts[i]=(int32_t)count;}
    if(r)return r;RocketCareOrder order={0};memcpy(order.potions,s->config.potion_order,sizeof(order.potions));memcpy(order.revives,s->config.revive_order,sizeof(order.revives));RocketDecision decision;uint32_t kind=rocket_care_ordered(s->team,health,&items,&order,true,&decision);
    if(kind==ROCKET_HEALTHY){if(s->preparing){s->preparing=0;s->team_ready=1;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","team ready hp=%d/%d,%d/%d,%d/%d",health[0].hp,health[0].max_hp,health[1].hp,health[1].max_hp,health[2].hp,health[2].max_hp);}else result(s,s->loss_recovery?7:0,true);return 0;}
    if(kind==ROCKET_NO_ITEMS){s->metrics.reason=9;phase(s,8,0,0);return 0;}
    if(kind!=ROCKET_HEAL&&kind!=ROCKET_REVIVE)return SILPH_MANAGED_CONTRACT;
    SilphManagedRef bag={0},promise={0};r=silph_game_read_item_bag(s->game,&bag);
    if(!r)r=silph_rocket_medicine(s->read,permit,bag,decision.incoming,(uint32_t)decision.item,&promise);
    if(!r)r=silph_managed_promise_open(s->medicine,silph_managed_promise_size(),s->read,s->managed,promise,&s->identity,++s->sequence,decision.incoming,now,"Holoholo.Rpc","PokemonProto");
    if(!r){s->medicine_active=1;s->healing_target=decision.incoming;for(unsigned i=0;i<3;i++)if(health[i].id==decision.incoming){s->healing_hp=(uint32_t)health[i].hp;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","team medicine requested slot=%u item=%d hp=%d/%d",i+1,decision.item,health[i].hp,health[i].max_hp);}}
    drop(s,&promise);drop(s,&bag);return r;
}
static int poll_rpc(Rocket *s,const SilphSchedulePermit *permit,uint64_t now){
    if(!s->rpc_active)return 0;SilphPromiseMetrics p={0};int r=silph_rpc_action_poll(s->rpc,&s->identity,now,&p);if(r||p.state==SILPH_PROMISE_WAITING)return r;
    if(p.state!=SILPH_PROMISE_RESOLVED)return SILPH_MANAGED_NOT_READY;
    SilphManagedRef reply={0};bool fresh=false;r=silph_rpc_action_take(s->rpc,&s->identity,now,&reply,&fresh);uint64_t code=0;
    if(!r&&!fresh)r=SILPH_MANAGED_STALE;
    const uint32_t kind=s->rpc_kind;
    if(!r)r=scalar(s,reply,kind==SILPH_RPC_ROCKET_CAPTURE||kind==SILPH_RPC_KECLEON_BERRY?"Status":"Result",SILPH_READ_ENUM32,&code,false);
    s->metrics.result=(uint32_t)code;
    if(!r&&kind==SILPH_RPC_KECLEON_BERRY){if(code==1){s->active_item=1;s->metrics.berries++;}}
    if(!r&&kind==SILPH_RPC_ROCKET_CAPTURE){
        if(code==1){uint64_t owned=0;r=scalar(s,reply,"CapturedPokemonId",SILPH_READ_U64,&owned,false);if(!r&&!owned)r=SILPH_MANAGED_CONTRACT;
            if(!r){s->metrics.captured=owned;s->metrics.catches++;s->metrics.shinies+=s->shiny;s->capture_finished_at=now;if(!rocket_capture_watch(&s->record_gate,owned,now)||!rocket_capture_watch(&s->capture_gate,owned,now))r=SILPH_MANAGED_CONTRACT;s->result_scan_at=0;s->result_blocked=0;uint64_t xp=0,dust=0;int rewards=silph_capture_rewards(s->read,s->managed,reply,&xp,&dust);
                if(rewards)s->metrics.reward_error=(uint32_t)rewards;else{s->metrics.xp+=xp;s->metrics.stardust+=dust;}
                if(!r)r=finish_encounter(s,permit,0);
                if(!r)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","capture confirmed; visual reward sequence bypassed request=%llu",(unsigned long long)s->metrics.request);
            }
        }else if(code==3||code==0){r=finish_encounter(s,permit,code==3?2:3);}else{s->active_item=0;s->berry_attempted=0;}
    }
    drop(s,&reply);int reset=silph_rpc_action_reset(s->rpc);s->rpc_active=0;s->rpc_kind=0;s->next=now+UINT64_C(50000000);return r?r:reset;
}
static int clean(Rocket *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool *busy){
    *busy=false;if(!(s->config.flags&SILPH_CFG_CLEAN)||s->metrics.phase==5||s->metrics.phase==6||now<s->clean_due)return 0;
    int r=0;if(!s->clean_opened){if(s->clean_index>=s->config.limit_count||s->clean_batches>=4){s->clean_index=s->clean_batches=0;s->clean_due=now+UINT64_C(15000000000);return 0;}
        r=silph_maintenance_open(s->maintenance,silph_maintenance_size(),s->read,s->managed,s->game,&s->identity);
        if(!r){s->clean_opened=1;s->clean_batches++;r=silph_maintenance_discard(s->maintenance,permit,snapshot,(int32_t)s->config.limits[s->clean_index].item,s->config.limits[s->clean_index].floor,50,now);}}
    SilphMaintenanceMetrics m={0};if(!r)r=silph_maintenance_poll(s->maintenance,&s->identity,now,&m);
    if(!r&&(m.state==3||m.state==4)){s->metrics.deleted+=m.discarded;if(!m.discarded||m.after<=m.floor)s->clean_index++;r=silph_maintenance_close(s->maintenance);if(!r)s->clean_opened=0;}
    *busy=!!s->clean_opened;return r;
}
static int capture_owner_match(Rocket *s,SilphManagedRef gui,uint64_t expected,bool *match){
    *match=false;if(!istype(s,gui,"Niantic.Holoholo.UI.Pokemon","PokemonInfoGuiController"))return 0;
    uint64_t state=0,id=0,post=0;SilphManagedRef pokemon={0},data={0};int r=scalar(s,gui,"currentState",SILPH_READ_ENUM32,&state,false);
    if(!r&&state!=2)return 0;
    if(!r)r=callref(s,gui,"get_CurrentPokemon",&pokemon);if(!r&&pokemon.serial)r=scalar(s,pokemon,"id_",SILPH_READ_U64,&id,false);
    if(!r&&id==expected){r=fieldref(s,gui,"egbx",&data);if(!r&&data.serial)r=scalar(s,data,"IsPostEncounter",SILPH_READ_BOOL,&post,false);}
    if(!r)*match=id==expected&&post;int a=drop(s,&data),b=drop(s,&pokemon);return r?r:a?a:b;
}
static int capture_record_owner(Rocket *s,const SilphSchedulePermit *permit,SilphManagedRef *out){
    const uint32_t layers[]={300,200,320,100,0,350,400,500,600,700,800,900,1000,1100};int r=0;*out=(SilphManagedRef){0};
    for(unsigned layer=0;!r&&!out->serial&&layer<14;layer++){
        SilphReadArgument argument={.kind=SILPH_READ_ENUM32,.value.bits=layers[layer]};SilphReadValue set={0},value={0};SilphManagedRef iterator={0},current={0};
        r=silph_rocket_invoke(s->read,permit,RK_LAYER,s->gui,&argument,1,&set);if(!r&&!set.reference.serial)continue;
        if(!r){r=silph_read_call(s->read,set.reference,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&value);if(r==SILPH_MANAGED_CONTRACT)r=silph_read_call(s->read,set.reference,"GetEnumerator",SILPH_READ_REFERENCE,NULL,0,&value);iterator=value.reference;}
        for(unsigned i=0;!r&&i<128;i++){
            uint64_t more=0;r=scalar(s,iterator,"MoveNext",SILPH_READ_BOOL,&more,true);if(r||!more)break;
            r=callref(s,iterator,"get_Current",&current);bool match=false;if(!r)r=capture_owner_match(s,current,s->capture_gate.captured,&match);
            if(!r&&match){*out=current;current=(SilphManagedRef){0};break;}int freed=drop(s,&current);if(!r)r=freed;
        }
        SilphManagedRef *refs[]={&current,&iterator,&set.reference};for(unsigned i=0;i<3;i++){int freed=drop(s,refs[i]);if(!r)r=freed;}
    }return r;
}
static int capture_result_ui(Rocket *s,const SilphSchedulePermit *permit,uint64_t now,bool *blocked){
    *blocked=!!s->result_blocked;if(!s->capture_gate.captured)return 0;if(now<s->result_scan_at)return 0;s->result_scan_at=now+UINT64_C(250000000);
    bool previous=*blocked;*blocked=false;SilphReadValue top={0};SilphManagedRef pokemon={0},data={0};uint64_t id=0,post=0;
    int r=silph_rocket_invoke(s->read,permit,RK_TOP,s->gui,NULL,0,&top);
    bool transition=!r&&(istype(s,top.reference,"Niantic.Holoholo.UI.Transitions","ScreenFadeGuiController")||istype(s,top.reference,"Niantic.Holoholo.Encounter.ArPlus","ArPlusSafetyPromptGuiController"));
    if(transition){*blocked=true;if(!s->transition_since){s->transition_since=now;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","post-capture transition blocked; waiting for game UI");}
        if(now-s->transition_since>=UINT64_C(20000000000)){s->metrics.reason=7;phase(s,8,0,0);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","post-capture transition timed out; requesting game recovery");}}
    else if(!r)s->transition_since=0;
    if(!r&&istype(s,top.reference,"Niantic.Holoholo.UI.Pokemon","PokemonInfoGuiController")){
        r=callref(s,top.reference,"get_CurrentPokemon",&pokemon);
        if(!r&&pokemon.serial)r=scalar(s,pokemon,"id_",SILPH_READ_U64,&id,false);
        if(!r&&id==s->capture_gate.captured){r=fieldref(s,top.reference,"egbx",&data);if(!r&&data.serial)r=scalar(s,data,"IsPostEncounter",SILPH_READ_BOOL,&post,false);}
        if(!r&&id==s->capture_gate.captured&&post){
            *blocked=true;uint32_t decision=rocket_capture_result(&s->capture_gate,id,true,now);
            if(decision==ROCKET_RESULT_DISMISS){SilphReadArgument expected={.kind=SILPH_READ_U64,.value.bits=id};r=action(s,permit,RK_CAPTURE_INFO,top.reference,&expected,1);
                if(r==SILPH_MANAGED_NOT_READY)r=0;
                else if(!r)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","captured result close requested attempt=%u request=%llu",s->capture_gate.attempts,(unsigned long long)s->metrics.request);
            }else if(decision==ROCKET_RESULT_EXPIRED){s->metrics.error=120;phase(s,8,0,0);}
        }
    }
    if(!r&&istype(s,top.reference,"Niantic.Holoholo.UI.Pokemon","PokemonSizeRecordBreakGuiController")){
        SilphManagedRef owner={0};r=capture_record_owner(s,permit,&owner);
        if(!r&&owner.serial){*blocked=true;uint32_t decision=rocket_capture_result(&s->record_gate,s->capture_gate.captured,true,now);
            if(decision==ROCKET_RESULT_DISMISS){SilphReadArgument args[]={{.kind=SILPH_READ_U64,.value.bits=s->capture_gate.captured},{.kind=SILPH_READ_REFERENCE,.value.reference=owner}};r=action(s,permit,RK_CAPTURE_RECORD,top.reference,args,2);if(r==SILPH_MANAGED_NOT_READY)r=0;else if(!r)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","capture size record dismissed request=%llu",(unsigned long long)s->metrics.request);}
            else if(decision==ROCKET_RESULT_EXPIRED){s->metrics.error=120;phase(s,8,0,0);}
        }
        int freed=drop(s,&owner);if(!r)r=freed;
    }
    if(!r&&!*blocked&&now>=s->capture_gate.until)memset(&s->capture_gate,0,sizeof(s->capture_gate));
    if(!r&&previous&&!*blocked){__android_log_print(ANDROID_LOG_INFO,"SilphRocket","captured result no longer blocking request=%llu",(unsigned long long)s->metrics.request);
        if(s->metrics.phase==3){s->deadline=now+UINT64_C(18000000000);s->scan_at=now;}
        else if(s->metrics.phase==5&&(s->metrics.stage==RS_OPEN||s->metrics.stage==RS_UI)){s->ui_started=now;s->ui_retry_at=now;s->deadline=now+UINT64_C(240000000000);}
    }
    s->result_blocked=*blocked;SilphManagedRef *refs[]={&data,&pokemon,&top.reference};for(unsigned i=0;i<3;i++){int freed=drop(s,refs[i]);if(!r)r=freed;}return r;
}
static int ready_to_open(Rocket *s,bool require_range,bool *ready){
    *ready=false;double lat=0,lon=0;int r=silph_game_read_rocket_position(s->game,&lat,&lon);
    if(r==SILPH_MANAGED_NOT_READY)return 0;if(r)return r;
    double dy=(lat-s->stop.latitude)*111195,dx=remainder(lon-s->stop.longitude,360.0)*111195*cos(s->stop.latitude*0.017453292519943295);
    if(hypot(dx,dy)>12)return 0;if(!require_range){*ready=true;return 0;}uint64_t in_range=0;r=scalar(s,s->place,"get_IsIncidentActiveAndInRange",SILPH_READ_BOOL,&in_range,true);if(!r)*ready=!!in_range;return r;
}
int silph_rocket_session_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,uint64_t wall,SilphRocketMetrics *out){
    Rocket *s=memory;if(!s||!s->opened||!out||!snapshot||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    silph_rocket_flow_permit(permit);
    if(!s->visual_attempted){s->visual_attempted=1;int tuning=silph_game_read_rocket_service(s->game,8,&s->visual_properties);if(!tuning)tuning=silph_rocket_battle_visuals(s->read,permit,s->visual_properties,s->visual_saved,false);if(!tuning)s->visual_tuned=1;else drop(s,&s->visual_properties);__android_log_print(ANDROID_LOG_INFO,"SilphRocket","battle transition timing prepared result=%d",tuning);}
    int r=0;for(unsigned i=0;i<8;i++){SilphKecleonCommand c;int popped=silph_session_link_pop(s->link,&c);if(popped==SILPH_SCHEDULE_BUSY||popped==SILPH_SCHEDULE_STATE)break;if(popped){r=popped;break;}r=command(s,&c,now);if(r)break;}
    if(!r&&s->metrics.phase==1&&s->dirty)silph_game_read_position(s->game,&s->latitude,&s->longitude);
    bool busy=false;if(!r)r=clean(s,permit,snapshot,now,&busy);if(!r&&!busy)r=poll_rpc(s,permit,now);
    bool closing=false;if(!r)r=capture_result_ui(s,permit,now,&closing);
    if(!r&&!busy&&!closing&&s->metrics.phase==3){refresh_map(s,permit,now);r=stop_at(s,now,wall);}
    if(!r&&!busy&&s->metrics.phase==5){
        if(!closing&&s->metrics.stage==RS_OPEN&&now>=s->ui_retry_at){
            SilphReadValue top={0};char current[161]={0};r=silph_rocket_invoke(s->read,permit,RK_TOP,s->gui,NULL,0,&top);
            bool prepared=!r&&(istype(s,top.reference,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")||istype(s,top.reference,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget"));
            int same=prepared?text(s,s->incident,"get_CurrentIncidentId",current,sizeof(current),true):0;
            prepared=prepared&&!same&&!strcmp(current,s->stop.incident);bool ready=false;if(!r)r=ready_to_open(s,!prepared,&ready);
            if(!r&&ready&&!prepared)r=action(s,permit,RK_TAP,s->place,NULL,0);
            int freed=drop(s,&top.reference);if(!r)r=freed;
            if(!r&&ready){s->metrics.stage=RS_UI;s->ui_taps=1;s->ui_retry_at=now+UINT64_C(2000000000);}
            else if(!r&&now-s->ui_started>=UINT64_C(12000000000))result(s,3,false);
        }
        if(!r&&s->metrics.stage!=RS_OPEN)r=ui(s,permit,now);
        if(!r&&!closing&&s->metrics.stage==RS_UI&&!s->ui_seen&&now>=s->ui_retry_at){
            if(now-s->ui_started>=UINT64_C(12000000000)||(s->ui_taps>=3&&!strcmp(s->top_name,"MainMenuGuiController"))){__android_log_print(ANDROID_LOG_INFO,"SilphRocket","Rocket opening not verified after %u attempts",s->ui_taps);result(s,3,false);}
            else if(!strcmp(s->top_name,"MainMenuGuiController")){bool ready=false;r=ready_to_open(s,true,&ready);if(!r&&ready){r=action(s,permit,RK_TAP,s->place,NULL,0);if(!r){s->ui_taps++;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","retrying same Rocket attempt=%u",s->ui_taps);}}s->ui_retry_at=now+UINT64_C(1500000000);}
        }
        if(!r&&s->metrics.stage==RS_LOSS){
            if(!strcmp(s->top_name,"MainMenuGuiController")){
                if(!s->loss_clear_at)s->loss_clear_at=now;
                if((s->loss_seen||now-s->loss_started>=UINT64_C(5000000000))&&now-s->loss_clear_at>=UINT64_C(1000000000)){s->metrics.stage=RS_HEAL;s->loss_recovery=0;__android_log_print(ANDROID_LOG_INFO,"SilphRocket","loss screens closed; healing team");}
            }else s->loss_clear_at=0;
            if(s->metrics.stage==RS_LOSS&&now>=s->loss_until){s->metrics.stage=RS_HEAL;s->loss_recovery=1;}
        }
        if(!r&&(s->metrics.stage==RS_UI||s->metrics.stage==RS_BATTLE))r=find_director(s,permit,now);
        if(!r&&s->director.serial&&(s->metrics.stage==RS_UI||s->metrics.stage==RS_BATTLE)){s->metrics.stage=RS_BATTLE;r=battle(s,permit,now);}
        if(!r&&(s->metrics.stage==RS_ENCOUNTER||(s->metrics.stage==RS_UI&&!strcmp(s->top_name,"EncounterGuiController"))))r=open_encounter(s,now);
        if(!r&&s->metrics.phase==5&&s->metrics.stage==RS_CAPTURE)r=capture(s,permit,snapshot,now);
        if(!r&&s->metrics.stage==RS_HEAL)r=care(s,permit,now);
    }
    if(!r&&!closing&&s->deadline&&now>s->deadline&&s->metrics.phase!=7&&s->metrics.phase!=8){if(s->metrics.phase==5){s->metrics.error=100+s->metrics.stage;phase(s,8,0,0);}else result(s,3,false);}
    if(r){s->metrics.error=(uint32_t)r;__android_log_print(ANDROID_LOG_ERROR,"SilphRocket","native request=%llu phase=%u stage=%u error=%d",(unsigned long long)s->metrics.request,s->metrics.phase,s->metrics.stage,r);}
    if(s->timing_stage!=s->metrics.stage||s->timing_request!=s->metrics.request){if(s->timing_at)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","stage completed request=%llu stage=%u ms=%llu next=%u",(unsigned long long)s->timing_request,s->timing_stage,(unsigned long long)((now-s->timing_at)/1000000),s->metrics.stage);s->timing_stage=s->metrics.stage;s->timing_request=s->metrics.request;s->timing_at=now;}
    *out=s->metrics;return r;
}
int silph_rocket_session_close(void *memory){Rocket *s=memory;if(!s||!s->opened)return 0;int r=release_candidate(s);
    if(s->visual_tuned){int restored=silph_rocket_battle_visuals(s->read,NULL,s->visual_properties,s->visual_saved,true);if(!r)r=restored;}int visual_drop=drop(s,&s->visual_properties);if(!r)r=visual_drop;
    if(s->map_settings.serial){int restored=silph_rocket_map_settings(s->read,NULL,s->map_settings,s->map_saved,true);if(!r)r=restored;}
    int ma=drop(s,&s->map_settings),mb=drop(s,&s->map_handler);if(!r)r=ma?ma:mb;
    SilphManagedRef *refs[]={&s->incident,&s->gui,&s->encounter_state};for(unsigned i=0;i<3;i++){int d=drop(s,refs[i]);if(!r)r=d;}
    int a=s->rpc?silph_rpc_action_close(s->rpc):0,b=s->clean_opened?silph_maintenance_close(s->maintenance):0,c=s->medicine?silph_managed_promise_close(s->medicine):0;
    if(!r)r=a?a:b?b:c;if(!r){free(s->rpc);free(s->maintenance);free(s->medicine);memset(s,0,sizeof(*s));}return r;
}
