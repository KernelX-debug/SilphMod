#include "kecleon-actions.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef SILPH_PILOT_SERVICE
#include "response-rewards.h"
#endif
typedef struct {
    void *read,*managed,*game,*rpc;
    SilphScheduleIdentity identity;
    SilphKecleonOptions options;
    SilphKecleonMetrics metrics;
    SilphGameStop stop;
    SilphRequestInput request;
    SilphKecleonCandidate candidate;
    char authorized_fort[161],authorized_incident[161];
    uint64_t started,scan_after,expires_ms;
    uint32_t opened,pending,active_item,berry_attempted,berry_suppressed;
} Kecleon;
int silph_kecleon_candidate_validate(const SilphKecleonCandidate *c){
    if(!c||!c->request||!isfinite(c->latitude)||!isfinite(c->longitude)||fabs(c->latitude)>90||fabs(c->longitude)>180||!c->source_id[0])return SILPH_MANAGED_ARGUMENT;
    unsigned length=0;while(length<sizeof(c->source_id)&&c->source_id[length]){char ch=c->source_id[length++];if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch=='.'||ch==':'))return SILPH_MANAGED_ARGUMENT;}
    return length<sizeof(c->source_id)?0:SILPH_MANAGED_ARGUMENT;
}
int silph_kecleon_bind_candidate(void *memory,const SilphKecleonCandidate *candidate){
    Kecleon *s=memory;if(!s||!s->opened||s->started||s->candidate.request)return SILPH_MANAGED_STATE;
    int status=silph_kecleon_candidate_validate(candidate);if(!status)s->candidate=*candidate;return status;
}
size_t silph_kecleon_actions_size(void){return sizeof(Kecleon);}
int silph_kecleon_bind_stop(void *memory,const char *fort,const char *incident){
    Kecleon *s=memory;if(!s||!s->opened||s->started||!s->candidate.request||!fort||!incident)return SILPH_MANAGED_STATE;
    size_t a=strnlen(fort,161),b=strnlen(incident,161);if(!a||a>=161||!b||b>=161)return SILPH_MANAGED_ARGUMENT;
    memcpy(s->authorized_fort,fort,a+1);memcpy(s->authorized_incident,incident,b+1);return 0;
}
bool silph_kecleon_inventory_due(void *memory){Kecleon *s=memory;return !s||!s->opened||(!s->pending&&s->metrics.state==SILPH_KECLEON_CAPTURE);}
int silph_kecleon_accept(uint32_t filter,uint32_t attack,uint32_t defense,uint32_t stamina,uint32_t shiny){
    if(filter>15||attack>15||defense>15||stamina>15||shiny>1)return -1;
    if(!filter)return 1;bool hundo=attack==15&&defense==15&&stamina==15,nundo=attack==0&&defense==0&&stamina==0;
    return ((filter&1)&&hundo)||((filter&2)&&nundo)||((filter&4)&&shiny)||((filter&8)&&shiny&&hundo);
}
static int berry_valid(uint32_t item){return item==0||item==701||item==703||item==705||item==706||item==708;}
int silph_kecleon_actions_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,const SilphKecleonOptions *options){
    if(!memory||bytes!=sizeof(Kecleon)||!read||!managed||!game||!identity||!options||options->abi!=1||options->filters>15||options->quality<1||options->quality>4
        ||options->curve>1||options->use_berries>1||options->berry_count>5||(options->use_berries&&!options->berry_count)||options->max_throws>100)return SILPH_MANAGED_ARGUMENT;
    for(unsigned i=0;i<options->berry_count;i++){if(!options->berries[i]||!berry_valid((uint32_t)options->berries[i]))return SILPH_MANAGED_ARGUMENT;
        for(unsigned j=0;j<i;j++)if(options->berries[j]==options->berries[i])return SILPH_MANAGED_ARGUMENT;}
    Kecleon *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);
    s->rpc=calloc(1,silph_rpc_action_size());if(!s->rpc)return SILPH_MANAGED_CAPACITY;
    s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->options=*options;s->metrics.state=SILPH_KECLEON_WAIT;s->opened=1;return 0;
}
static int field(Kecleon *s,SilphManagedRef result,const char *name,uint32_t kind,uint64_t *output){
    SilphReadValue value={0};int status=silph_read_field(s->read,result,name,kind,&value);if(!status)*output=value.bits;return status;
}
static int drop(Kecleon *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)memset(ref,0,sizeof(*ref));return r;}
static double distance(double a,double b,double c,double d){
    double r=.017453292519943295,x=sin((a-c)*r/2),y=sin((b-d)*r/2),h=x*x+cos(a*r)*cos(c*r)*y*y;return 12742000*asin(sqrt(fmax(0,fmin(1,h))));
}
static int send(Kecleon *s,const SilphSchedulePermit *permit,uint32_t operation,uint64_t now){
    double lat=0,lon=0;int status=silph_game_read_position(s->game,&lat,&lon);
    if(!status&&distance(lat,lon,s->stop.latitude,s->stop.longitude)>50){s->metrics.state=SILPH_KECLEON_UNVERIFIED;return 0;}
    uint64_t ticket=s->metrics.encounter?s->metrics.encounter:UINT64_C(1);
    if(!status)status=silph_rpc_action_send(s->rpc,permit,operation,&s->request,ticket,now);
    if(!status){s->pending=operation;s->metrics.requests++;if(operation==SILPH_RPC_KECLEON_CAPTURE)s->metrics.throws++;}
    return status;
}
static int read_get(Kecleon *s,SilphManagedRef result,uint64_t wall){
    uint64_t species=0,id=0,expires=0,a=0,b=0;int status=field(s,result,"PokemonId",SILPH_READ_ENUM32,&species);
    if(!status)status=field(s,result,"EncounterId",SILPH_READ_U64,&id);
    if(!status)status=field(s,result,"DisappearTimeMs",SILPH_READ_I64,&expires);
    if(!status)status=field(s,result,"Lat",SILPH_READ_F64,&a);
    if(!status)status=field(s,result,"Lng",SILPH_READ_F64,&b);
    double lat=0,lon=0;memcpy(&lat,&a,8);memcpy(&lon,&b,8);
    if(!status&&(species!=352||!id||expires<=wall||expires>INT64_MAX||!isfinite(lat)||!isfinite(lon)||fabs(lat)>90||fabs(lon)>180
        ||distance(lat,lon,s->stop.latitude,s->stop.longitude)>50))status=SILPH_MANAGED_CONTRACT;
    SilphReadValue text={0};uint32_t length=0;
    if(!status)status=silph_read_field(s->read,result,"EncounterLocation",SILPH_READ_REFERENCE,&text);
    if(!status)status=silph_read_utf8(s->read,text.reference,s->request.location,sizeof(s->request.location),&length);
    if(!status){s->request.encounter=id;s->metrics.encounter=id;s->expires_ms=expires;status=silph_proto_validate(SILPH_RPC_KECLEON_OPEN,&s->request);}
    int freed=drop(s,&text.reference);return status?status:freed;
}
static int read_open(Kecleon *s,SilphManagedRef result){
    SilphReadValue pokemon={0},display={0};int status=silph_read_field(s->read,result,"Pokemon",SILPH_READ_REFERENCE,&pokemon);
    if(!status)status=silph_read_is_type(s->read,pokemon.reference,"Holoholo.Rpc","PokemonProto");
    const char *names[]={"pokemonId_","cp_","individualAttack_","individualDefense_","individualStamina_"};uint32_t data[5]={0};
    for(unsigned i=0;!status&&i<5;i++){uint64_t value=0;status=field(s,pokemon.reference,names[i],SILPH_READ_I32,&value);data[i]=(uint32_t)value;}
    uint64_t id=0,shiny=0,active=0;
    if(!status)status=field(s,pokemon.reference,"id_",SILPH_READ_U64,&id);
    if(!status)status=silph_read_field(s->read,pokemon.reference,"pokemonDisplay_",SILPH_READ_REFERENCE,&display);
    if(!status)status=silph_read_is_type(s->read,display.reference,"Holoholo.Rpc","PokemonDisplayProto");
    if(!status)status=field(s,display.reference,"Shiny",SILPH_READ_BOOL,&shiny);
    if(!status)status=field(s,result,"ActiveItem",SILPH_READ_ENUM32,&active);
    int accepted=silph_kecleon_accept(s->options.filters,data[2],data[3],data[4],(uint32_t)shiny);
    if(!status&&(data[0]!=352||!id||data[1]>100000||accepted<0||!berry_valid((uint32_t)active)))status=SILPH_MANAGED_CONTRACT;
    if(!status){s->metrics.cp=data[1];s->metrics.attack=data[2];s->metrics.defense=data[3];s->metrics.stamina=data[4];s->metrics.shiny=(uint32_t)shiny;
        s->metrics.encounters++;s->metrics.shinies+=shiny?1:0;s->metrics.verified=1;s->active_item=(uint32_t)active;
        if(!accepted){s->metrics.filtered=1;s->metrics.state=SILPH_KECLEON_DONE;}else s->metrics.state=SILPH_KECLEON_CAPTURE;}
    int a=drop(s,&display.reference),b=drop(s,&pokemon.reference);return status?status:a?a:b;
}
int silph_kecleon_actions_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,uint64_t wall,SilphKecleonMetrics *output){
    if(!output||!snapshot)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));Kecleon *s=memory;
    if(!s||!s->opened||!silph_schedule_permitted(permit,true)||!now||!wall)return SILPH_MANAGED_STATE;
    int status=0;
    if(!s->started){s->started=now;s->identity.account_hi=snapshot->account_hi;s->identity.account_lo=snapshot->account_lo;
        status=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),s->read,s->managed,s->game,&s->identity);}
    if(!status&&(s->identity.account_hi!=snapshot->account_hi||s->identity.account_lo!=snapshot->account_lo))status=SILPH_MANAGED_STALE;
    if(!status&&(now-s->started>=UINT64_C(180000000000)||(s->expires_ms&&wall>=s->expires_ms))){s->metrics.state=SILPH_KECLEON_UNVERIFIED;if(s->pending)s->metrics.uncertain=1;}
    if(!status&&s->metrics.state==SILPH_KECLEON_WAIT&&now>=s->scan_after){
        s->scan_after=now+UINT64_C(1000000000);
        status=s->candidate.request?silph_game_read_stop_at(s->game,50,s->candidate.latitude,s->candidate.longitude,3,&s->stop):silph_game_read_stop(s->game,50,8,NULL,NULL,&s->stop);
        if(status==SILPH_MANAGED_NOT_READY)status=0;
        else if(!status){
            if(s->candidate.request&&(!s->stop.scan_complete||s->stop.matches!=1))s->metrics.state=SILPH_KECLEON_WAIT;
            else if(s->candidate.request&&(s->stop.incident_type!=8||!s->stop.active||s->stop.complete))s->metrics.state=SILPH_KECLEON_WAIT;
            else if(s->authorized_fort[0]&&(strcmp(s->authorized_fort,s->stop.fort)||strcmp(s->authorized_incident,s->stop.incident)))s->metrics.state=SILPH_KECLEON_UNVERIFIED;
            else if(!s->stop.active||s->stop.complete||!s->stop.incident[0]||s->stop.expires_ms<=wall)s->metrics.state=SILPH_KECLEON_UNVERIFIED;
            else{s->request=(SilphRequestInput){.abi=1,.latitude=s->stop.latitude,.longitude=s->stop.longitude,.quality=s->options.quality,.curve=s->options.curve};
                memcpy(s->request.fort,s->stop.fort,sizeof(s->request.fort));s->metrics.state=SILPH_KECLEON_GET;status=send(s,permit,SILPH_RPC_KECLEON_GET,now);}
        }
    }else if(!status&&s->pending&&s->metrics.state!=SILPH_KECLEON_UNVERIFIED){
        SilphPromiseMetrics promise;status=silph_rpc_action_poll(s->rpc,&s->identity,now,&promise);
        if(!status&&promise.state==SILPH_PROMISE_RESOLVED){
            SilphManagedRef result={0};bool fresh=false;uint64_t code=0;uint32_t operation=s->pending;
            status=silph_rpc_action_take(s->rpc,&s->identity,now,&result,&fresh);
            if(!status)status=field(s,result,operation==SILPH_RPC_KECLEON_OPEN?"Result":"Status",SILPH_READ_ENUM32,&code);
            if(!status){s->metrics.status=(uint32_t)code;
                if(operation==SILPH_RPC_KECLEON_GET){
                    if(code==1){status=read_get(s,result,wall);if(!status)s->metrics.state=SILPH_KECLEON_OPEN;}
                    else if(code==2||code==3){s->metrics.verified=1;s->metrics.state=SILPH_KECLEON_DONE;}
                    else s->metrics.state=SILPH_KECLEON_UNVERIFIED;
                }else if(operation==SILPH_RPC_KECLEON_OPEN){if(code==1)status=read_open(s,result);else s->metrics.state=SILPH_KECLEON_UNVERIFIED;}
                else if(operation==SILPH_RPC_KECLEON_CAPTURE){
                    if(code==1){uint64_t id=0;status=field(s,result,"CapturedPokemonId",SILPH_READ_U64,&id);if(!status&&!id)status=SILPH_MANAGED_CONTRACT;
                        if(!status){s->metrics.captured_id=id;s->metrics.catches=1;s->metrics.fresh_capture=fresh;s->metrics.state=SILPH_KECLEON_DONE;
#ifdef SILPH_PILOT_SERVICE
                            s->metrics.reward_error=(uint32_t)silph_capture_rewards(s->read,s->managed,result,&s->metrics.xp,&s->metrics.stardust);
#endif
                        }}
                    else if(code==2||code==4){if(code==2){s->active_item=0;s->berry_attempted=0;}s->metrics.state=SILPH_KECLEON_CAPTURE;}
                    else if(code==3)s->metrics.state=SILPH_KECLEON_DONE;else s->metrics.state=SILPH_KECLEON_UNVERIFIED;
                }else{
                    uint64_t active=0;status=field(s,result,"ActiveItem",SILPH_READ_ENUM32,&active);
                    if(!status&&!berry_valid((uint32_t)active))status=SILPH_MANAGED_CONTRACT;
                    if(!status){if(code==0&&active==(uint32_t)s->request.item){s->active_item=(uint32_t)active;s->metrics.berries++;}
                        else if(code==2&&active)s->active_item=(uint32_t)active;else s->berry_suppressed=1;
                        s->metrics.state=code==1?SILPH_KECLEON_DONE:SILPH_KECLEON_CAPTURE;}
                }
            }
            int a=drop(s,&result),b=silph_rpc_action_reset(s->rpc);s->pending=0;if(!status)status=a?a:b;
        }else if(!status&&promise.state!=SILPH_PROMISE_WAITING){s->metrics.state=SILPH_KECLEON_UNVERIFIED;s->metrics.uncertain=1;}
    }else if(!status&&!s->pending&&s->metrics.state==SILPH_KECLEON_OPEN)status=send(s,permit,SILPH_RPC_KECLEON_OPEN,now);
    else if(!status&&!s->pending&&s->metrics.state==SILPH_KECLEON_CAPTURE){
        if(snapshot->pokemon_count>=snapshot->max_pokemon||(s->options.max_throws&&s->metrics.throws>=s->options.max_throws))s->metrics.state=SILPH_KECLEON_UNVERIFIED;
        else{
            unsigned ball=0;for(unsigned i=0;i<3;i++)if(snapshot->items[i]){ball=i+1;break;}
            if(!ball)s->metrics.state=SILPH_KECLEON_UNVERIFIED;
            else{
                int32_t berry=0;const int32_t ids[]={701,703,705,706,708};
                if(s->options.use_berries&&!s->active_item&&!s->berry_attempted&&!s->berry_suppressed){
                    for(unsigned i=0;!berry&&i<s->options.berry_count;i++)for(unsigned j=0;j<5;j++)if(s->options.berries[i]==ids[j]&&snapshot->items[j+3])berry=ids[j];
                }
                s->request.item=berry?berry:(int32_t)ball;
                if(berry){s->berry_attempted=1;s->metrics.state=SILPH_KECLEON_BERRY;status=send(s,permit,SILPH_RPC_KECLEON_BERRY,now);}
                else status=send(s,permit,SILPH_RPC_KECLEON_CAPTURE,now);
            }
        }
    }
    if(status)s->metrics.error=(uint32_t)status;*output=s->metrics;return status;
}
int silph_kecleon_actions_close(void *memory){
    Kecleon *s=memory;if(!s||!s->opened)return 0;int status=silph_rpc_action_close(s->rpc);if(status)return status;free(s->rpc);memset(s,0,sizeof(*s));return 0;
}
