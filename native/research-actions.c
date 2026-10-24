#include "research-actions.h"
#include "research-policy.h"
#include "managed-promise.h"
#include "response-rewards.h"
#include <stdlib.h>
#include <string.h>
typedef struct {char id[257];uint32_t kind,terminal;uint64_t until;} Entry;
typedef struct {
    void *read,*managed,*game,*pending;SilphScheduleIdentity identity;SilphManagedRef quests,rpc,factory,reward;
    uint32_t opened,transfers,berries,kind;uint64_t sequence,due,after,encounter;char quest_id[257];
    Entry entries[256];SilphResearchMetrics metrics;
} Research;
size_t silph_research_size(void){return sizeof(Research);}
static int drop(Research *s,SilphManagedRef *ref){return silph_reward_drop(s->managed,ref);}
static int integer(Research *s,SilphManagedRef ref,const char *name,uint32_t *out){SilphReadValue v={0};int status=silph_read_field(s->read,ref,name,SILPH_READ_ENUM32,&v);if(!status&&v.bits>INT32_MAX)status=SILPH_MANAGED_CONTRACT;if(!status)*out=(uint32_t)v.bits;return status;}
static int text(Research *s,SilphManagedRef ref,const char *field,char *out){SilphReadValue v={0};int status=silph_read_field(s->read,ref,field,SILPH_READ_REFERENCE,&v);uint32_t bytes=0;if(!status)status=silph_read_utf8(s->read,v.reference,out,257,&bytes);if(!status&&!bytes)status=SILPH_MANAGED_CONTRACT;int freed=drop(s,&v.reference);return status?status:freed;}
static int call(Research *s,const SilphSchedulePermit *p,uint32_t op,SilphManagedRef ref,SilphManagedRef input,SilphReadValue *out){
    s->metrics.operation=op;SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=input};
    int status=silph_research_invoke(s->read,p,op,ref,input.serial?&arg:NULL,input.serial?1:0,out);if(status&&out->bits>=40&&out->bits<=80)s->metrics.operation=(uint32_t)out->bits;return status;
}
static int count(Research *s,SilphManagedRef list,uint32_t limit,uint32_t *out){SilphReadValue v={0};int r=silph_read_call(s->read,list,"get_Count",SILPH_READ_I32,NULL,0,&v);if(!r&&v.bits>limit)r=SILPH_MANAGED_CONTRACT;if(!r)*out=(uint32_t)v.bits;return r;}
static int item(Research *s,SilphManagedRef list,uint32_t i,const char *name,SilphManagedRef *out){SilphReadArgument arg={.kind=SILPH_READ_I32,.value.bits=i};SilphReadValue v={0};int r=silph_read_call(s->read,list,"get_Item",SILPH_READ_REFERENCE,&arg,1,&v);if(!r)r=silph_read_is_type(s->read,v.reference,"Holoholo.Rpc",name);if(!r)*out=v.reference;else drop(s,&v.reference);return r;}
static bool available(Research *s,uint32_t kind,const char *id,uint64_t now){
    bool empty=false;for(unsigned i=0;i<256;i++){Entry *e=&s->entries[i];if(!e->id[0])empty=true;else if(e->kind==kind&&!strcmp(e->id,id))return !e->terminal&&now>=e->until;}
    return empty;
}
static int record(Research *s,uint32_t kind,const char *id,uint64_t now,uint32_t terminal){
    Entry *chosen=NULL;for(unsigned i=0;i<256;i++){Entry *e=&s->entries[i];if(!e->id[0]&&!chosen)chosen=e;if(e->kind==kind&&!strcmp(e->id,id)){chosen=e;break;}}
    if(!chosen)return SILPH_MANAGED_CAPACITY;memset(chosen,0,sizeof(*chosen));memcpy(chosen->id,id,strlen(id)+1);chosen->kind=kind;chosen->terminal=terminal;chosen->until=now+UINT64_C(60000000000);return 0;
}
static int reserve(Research *s,SilphManagedRef promise,uint32_t kind,const char *id,uint64_t now,const char *response){
    int status=record(s,kind,id,now,0);s->after=now+UINT64_C(3000000000);s->kind=kind;memcpy(s->quest_id,id,strlen(id)+1);
    uint64_t sequence=++s->sequence;
    if(!status)status=silph_managed_promise_open(s->pending,silph_managed_promise_size(),s->read,s->managed,promise,&s->identity,sequence,kind==3?s->encounter:sequence,now,"Holoholo.Rpc",response);
    int freed=drop(s,&promise);if(!status)status=freed;if(!status)s->metrics.state=2;return status;
}
int silph_research_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *id,uint32_t transfers,uint32_t berries){
    if(!memory||bytes!=sizeof(Research)||!read||!managed||!game||!id||transfers>1||berries>1)return SILPH_MANAGED_ARGUMENT;
    Research *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*id;s->transfers=transfers;s->berries=berries;s->opened=1;
    s->pending=calloc(1,silph_managed_promise_size());int status=s->pending?0:SILPH_MANAGED_CAPACITY;
    if(!status){s->metrics.operation=11;status=silph_game_read_research_service(game,0,&s->quests);}
    if(!status){s->metrics.operation=12;status=silph_game_read_research_service(game,1,&s->rpc);}
    if(!status){s->metrics.operation=13;status=silph_game_read_research_service(game,2,&s->factory);}
    s->metrics.state=status?3:1;s->metrics.error=(uint32_t)status;return 0;
}
bool silph_research_due(void *memory,uint64_t now){Research *s=memory;return s&&s->opened&&s->metrics.state==1&&now>=s->due&&now>=s->after;}
bool silph_research_pending(void *memory){Research *s=memory;return s&&s->opened&&s->metrics.state==2;}
void silph_research_refresh(void *memory,uint64_t now){Research *s=memory;if(s&&s->opened){uint64_t next=now+UINT64_C(500000000);if(s->due>next)s->due=next;}}
static int task(Research *s,const SilphSchedulePermit *permit,SilphManagedRef quest,uint32_t *decision,bool *receivable){
    uint32_t context=0,type=0,status=0,conditions[32]={0},condition_count=0;SilphReadValue redeem={0},goal={0},list={0},rewards={0};*decision=0;*receivable=false;
    s->metrics.operation=20;int error=integer(s,quest,"QuestContext",&context);if(!error&&context!=2)return 0;
    if(!error){s->metrics.operation=21;error=integer(s,quest,"QuestType",&type);}if(!error){s->metrics.operation=22;error=integer(s,quest,"Status",&status);}
    if(!error)error=call(s,permit,SILPH_RESEARCH_REDEEM,(SilphManagedRef){0},quest,&redeem);
    if(!error&&!redeem.bits){
        s->metrics.operation=23;error=silph_read_field(s->read,quest,"Goal",SILPH_READ_REFERENCE,&goal);
        if(!error&&goal.reference.serial){s->metrics.operation=24;error=silph_read_is_type(s->read,goal.reference,"Holoholo.Rpc","QuestGoalProto");
            if(!error){s->metrics.operation=25;error=silph_read_field(s->read,goal.reference,"Condition",SILPH_READ_REFERENCE,&list);}
            if(!error){s->metrics.operation=26;error=count(s,list.reference,32,&condition_count);}
            for(unsigned i=0;!error&&i<condition_count;i++){SilphManagedRef condition={0};s->metrics.operation=27;error=item(s,list.reference,i,"QuestConditionProto",&condition);if(!error){s->metrics.operation=28;error=integer(s,condition,"Type",&conditions[i]);}int freed=drop(s,&condition);if(!error)error=freed;}
        }
    }
    if(!error){*decision=(uint32_t)silph_research_decide(context,type,status,(uint32_t)redeem.bits,conditions,condition_count,s->transfers,s->berries);*receivable=true;}
    if(!error&&*decision==SILPH_RESEARCH_CLAIM_TASK){
        error=silph_read_field(s->read,quest,"QuestRewards",SILPH_READ_REFERENCE,&rewards);uint32_t n=0;if(!error)error=count(s,rewards.reference,32,&n);
        for(uint32_t i=0;!error&&i<n;i++){SilphManagedRef reward={0};SilphReadValue result={0};error=item(s,rewards.reference,i,"QuestRewardProto",&reward);
            if(!error)error=call(s,permit,SILPH_RESEARCH_RECEIVE,s->quests,reward,&result);if(!error&&!result.bits)*receivable=false;
            int freed=drop(s,&reward);if(!error)error=freed;
        }
    }
    int a=drop(s,&goal.reference),b=drop(s,&list.reference),c=drop(s,&rewards.reference);return error?error:a?a:b?b:c;
}
static int scan(Research *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool capture){
    SilphReadValue busy={0},list={0},reward={0};SilphManagedRef selected={0},remove={0};char ids[17][257]={{0}},selected_id[257]={0},remove_id[257]={0};uint32_t n=0;
    int status=call(s,permit,SILPH_RESEARCH_BUSY,s->quests,(SilphManagedRef){0},&busy);s->due=now+UINT64_C(10000000000);if(!status&&busy.bits)return 0;
    if(!status)status=call(s,permit,SILPH_RESEARCH_LIST,s->quests,(SilphManagedRef){0},&list);if(!status)status=count(s,list.reference,16,&n);if(!status)s->metrics.scans++;
    for(uint32_t i=0;!status&&i<n;i++){
        SilphManagedRef quest={0};uint32_t decision=0;bool receivable=false;status=item(s,list.reference,i,"QuestProto",&quest);
        if(!status)status=text(s,quest,"QuestId",ids[i]);if(!status)status=task(s,permit,quest,&decision,&receivable);
        if(!status&&decision==1&&receivable&&!selected.serial&&available(s,1,ids[i],now)){selected=quest;quest=(SilphManagedRef){0};memcpy(selected_id,ids[i],257);}
        if(!status&&decision==2&&!remove.serial&&available(s,2,ids[i],now)){remove=quest;quest=(SilphManagedRef){0};memcpy(remove_id,ids[i],257);}
        int freed=drop(s,&quest);if(!status)status=freed;
    }
    if(!status)status=call(s,permit,SILPH_RESEARCH_REWARD,s->quests,(SilphManagedRef){0},&reward);
    uint32_t encounter_type=0,ball=0;uint64_t encounter=0;
    if(!status&&reward.reference.serial){SilphReadValue pokemon={0},id={0};status=silph_read_is_type(s->read,reward.reference,"Holoholo.Rpc","QuestPokemonEncounterProto");
        if(!status)status=text(s,reward.reference,"QuestId",ids[16]);if(!status)status=integer(s,reward.reference,"EncounterType",&encounter_type);
        if(!status)status=integer(s,reward.reference,"PokeBallOverride",&ball);if(!status&&encounter_type!=6)status=SILPH_MANAGED_CONTRACT;
        if(!status)status=silph_read_field(s->read,reward.reference,"Pokemon",SILPH_READ_REFERENCE,&pokemon);
        if(!status)status=silph_read_is_type(s->read,pokemon.reference,"Holoholo.Rpc","PokemonProto");
        if(!status)status=silph_read_call(s->read,pokemon.reference,"get_Id",SILPH_READ_U64,NULL,0,&id);encounter=id.bits;
        if(!status&&!encounter)status=SILPH_MANAGED_CONTRACT;int freed=drop(s,&pokemon.reference);if(!status)status=freed;
    }
    if(!status)for(unsigned i=0;i<256;i++){Entry *e=&s->entries[i];if(e->id[0]&&now>=e->until){bool present=false;for(unsigned j=0;j<17;j++)if(!strcmp(ids[j],e->id))present=true;if(!present)memset(e,0,sizeof(*e));}}
    if(!status&&(selected.serial||remove.serial)){
        uint32_t kind=selected.serial?1:2,op=selected.serial?SILPH_RESEARCH_CLAIM:SILPH_RESEARCH_REMOVE;SilphReadValue pending={0};
        status=call(s,permit,op,s->rpc,selected.serial?selected:remove,&pending);
        if(!status){s->metrics.requests++;status=reserve(s,pending.reference,kind,selected.serial?selected_id:remove_id,now,kind==1?"CompleteQuestOutProto":"RemoveQuestOutProto");}
    }else if(!status&&capture&&reward.reference.serial&&!ball&&snapshot->pokemon_count<snapshot->max_pokemon&&(snapshot->items[0]||snapshot->items[1]||snapshot->items[2])&&available(s,3,ids[16],now)){
        SilphReadValue pending={0};s->encounter=encounter;status=call(s,permit,SILPH_RESEARCH_BEGIN,s->rpc,reward.reference,&pending);
        if(!status){s->metrics.requests++;s->reward=reward.reference;reward.reference=(SilphManagedRef){0};status=reserve(s,pending.reference,3,ids[16],now,"QuestEncounterOutProto");}
    }
    int a=drop(s,&list.reference),b=drop(s,&reward.reference),c=drop(s,&selected),d=drop(s,&remove);return status?status:a?a:b?b:c?c:d;
}
static int poll(Research *s,const SilphSchedulePermit *permit,uint64_t now,SilphManagedRef *target,uint64_t *encounter,uint32_t *species){
    SilphPromiseMetrics pending={0};int status=silph_managed_promise_poll(s->pending,&s->identity,now,&pending);if(!status&&pending.state==SILPH_PROMISE_WAITING)return 0;
    if(!status&&pending.state!=SILPH_PROMISE_RESOLVED)status=SILPH_MANAGED_STATE;
    SilphManagedRef result={0};bool fresh=false;if(!status)status=silph_managed_promise_take(s->pending,&s->identity,now,&result,&fresh);if(!status&&!fresh)status=SILPH_MANAGED_STALE;
    uint32_t code=0;if(!status)status=integer(s,result,s->kind==3?"Result":"Status",&code);s->metrics.result=code;
    if(!status&&code==1){
        if(s->kind==1){uint64_t xp=0,dust=0;int rewards=silph_quest_rewards(s->read,s->managed,result,&xp,&dust);if(rewards)s->metrics.reward_error=(uint32_t)rewards;else{s->metrics.xp+=xp;s->metrics.stardust+=dust;}s->metrics.claimed++;}
        else if(s->kind==2)s->metrics.discarded++;
        else{
            SilphReadArgument args[]={{.kind=SILPH_READ_REFERENCE,.value.reference=s->reward},{.kind=SILPH_READ_REFERENCE,.value.reference=result}};SilphReadValue object={0},id={0},pokemon={0},dex={0};
            s->metrics.operation=SILPH_RESEARCH_CREATE;status=silph_research_invoke(s->read,permit,SILPH_RESEARCH_CREATE,s->factory,args,2,&object);
            if(!status)status=silph_read_is_type(s->read,object.reference,"Niantic.Holoholo.Quests","QuestMapPokemon");
            if(!status)status=silph_read_call(s->read,object.reference,"get_EncounterId",SILPH_READ_U64,NULL,0,&id);if(!status&&id.bits!=s->encounter)status=SILPH_MANAGED_STALE;
            if(!status)status=silph_read_field(s->read,result,"Pokemon",SILPH_READ_REFERENCE,&pokemon);
            if(!status)status=silph_read_is_type(s->read,pokemon.reference,"Holoholo.Rpc","PokemonProto");
            if(!status)status=silph_read_field(s->read,pokemon.reference,"pokemonId_",SILPH_READ_I32,&dex);if(!status&&(!dex.bits||dex.bits>=2048))status=SILPH_MANAGED_CONTRACT;
            if(!status){*target=object.reference;object.reference=(SilphManagedRef){0};*encounter=s->encounter;*species=(uint32_t)dex.bits;s->metrics.encounters++;}
            if(object.reference.serial){silph_research_destroy(s->read,object.reference);drop(s,&object.reference);}int freed=drop(s,&pokemon.reference);if(!status)status=freed;
        }
    }
    if(!status)status=record(s,s->kind,s->quest_id,now,code==1||(s->kind==1&&(code==2||code==4))||(s->kind==2&&code==2));
    int a=drop(s,&result),b=drop(s,&s->reward),c=silph_managed_promise_close(s->pending);if(!status)status=a?a:b?b:c;
    s->kind=0;s->metrics.state=status?3:1;silph_research_refresh(s,now);return status;
}
int silph_research_tick(void *memory,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool capture,SilphManagedRef *target,uint64_t *encounter,uint32_t *species){
    if(!target||!encounter||!species)return SILPH_MANAGED_ARGUMENT;*target=(SilphManagedRef){0};*encounter=0;*species=0;Research *s=memory;
    if(!s||!s->opened||!snapshot||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(s->metrics.state==3)return 0;
    int status=s->metrics.state==2?poll(s,permit,now,target,encounter,species):silph_research_due(s,now)?scan(s,permit,snapshot,now,capture):0;
    if(status){s->metrics.error=(uint32_t)status;s->metrics.state=3;if(s->pending)silph_managed_promise_close(s->pending);drop(s,&s->reward);}
    return 0;
}
int silph_research_metrics(void *memory,SilphResearchMetrics *out){Research *s=memory;if(!s||!s->opened||!out)return SILPH_MANAGED_ARGUMENT;*out=s->metrics;return 0;}
int silph_research_close(void *memory){Research *s=memory;if(!s||!s->opened)return 0;int status=s->pending?silph_managed_promise_close(s->pending):0;
    SilphManagedRef *refs[]={&s->quests,&s->rpc,&s->factory,&s->reward};for(unsigned i=0;i<4;i++){int freed=drop(s,refs[i]);if(!status)status=freed;}if(!status){free(s->pending);memset(s,0,sizeof(*s));}return status;
}
