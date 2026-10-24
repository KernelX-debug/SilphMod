#ifndef SILPH_RESPONSE_REWARDS_H
#define SILPH_RESPONSE_REWARDS_H
#include "managed-read.h"
static inline int silph_reward_drop(void *managed,SilphManagedRef *ref){if(!ref->serial)return 0;int s=silph_managed_release(managed,*ref);if(!s)*ref=(SilphManagedRef){0};return s;}
static inline int silph_reward_integer(void *read,SilphManagedRef object,const char *field,uint64_t *out){SilphReadValue v={0};int s=silph_read_field(read,object,field,SILPH_READ_I32,&v);if(!s&&v.bits>10000000)s=SILPH_MANAGED_CONTRACT;if(!s)*out=v.bits;return s;}
static inline int silph_reward_sum(void *read,void *managed,SilphManagedRef scores,const char *field,uint64_t *out){
    SilphReadValue list={0},count={0};int status=silph_read_field(read,scores,field,SILPH_READ_REFERENCE,&list);
    if(!status)status=silph_read_call(read,list.reference,"get_Count",SILPH_READ_I32,NULL,0,&count);if(!status&&count.bits>64)status=SILPH_MANAGED_CONTRACT;
    uint64_t sum=0;for(uint32_t i=0;!status&&i<count.bits;i++){SilphReadArgument index={.kind=SILPH_READ_I32,.value.bits=i};SilphReadValue value={0};status=silph_read_call(read,list.reference,"get_Item",SILPH_READ_I32,&index,1,&value);if(!status&&(value.bits>10000000||sum>10000000-value.bits))status=SILPH_MANAGED_CONTRACT;if(!status)sum+=value.bits;}
    int freed=silph_reward_drop(managed,&list.reference);if(!status)status=freed;if(!status)*out=sum;return status;
}
static inline int silph_capture_rewards(void *read,void *managed,SilphManagedRef result,uint64_t *xp,uint64_t *dust){
    SilphReadValue scores={0},temp={0};uint64_t a=0,b=0,mega=0,evo=0;int status=silph_read_is_type(read,result,"Holoholo.Rpc","CatchPokemonOutProto");
    if(!status)status=silph_read_field(read,result,"Scores",SILPH_READ_REFERENCE,&scores);
    if(!status)status=silph_read_is_type(read,scores.reference,"Holoholo.Rpc","CaptureScoreProto");
    if(!status)status=silph_reward_sum(read,managed,scores.reference,"Exp",&a);
    if(!status)status=silph_reward_sum(read,managed,scores.reference,"Stardust",&b);
    if(!status)status=silph_reward_integer(read,scores.reference,"ExperienceFromActiveMega",&mega);
    if(!status)status=silph_read_field(read,scores.reference,"TempEvoScoreInfo",SILPH_READ_REFERENCE,&temp);
    if(!status&&temp.reference.serial)status=silph_reward_integer(read,temp.reference,"ExperienceFromActiveTempEvo",&evo);
    if(!status&&(a+mega+evo>10000000))status=SILPH_MANAGED_CONTRACT;
    int x=silph_reward_drop(managed,&temp.reference),y=silph_reward_drop(managed,&scores.reference);if(!status)status=x?x:y;
    if(!status){*xp=a+mega+evo;*dust=b;}return status;
}
static inline int silph_reward_loot(void *read,void *managed,SilphManagedRef result,const char *name,uint64_t *xp,uint64_t *dust){
    SilphReadValue loot={0},list={0},count={0},item={0};int status=silph_read_field(read,result,name,SILPH_READ_REFERENCE,&loot);
    if(!status&&!loot.reference.serial)return 0;
    if(!status)status=silph_read_is_type(read,loot.reference,"Holoholo.Rpc","LootProto");
    if(!status)status=silph_read_field(read,loot.reference,"LootItem",SILPH_READ_REFERENCE,&list);
    if(!status)status=silph_read_call(read,list.reference,"get_Count",SILPH_READ_I32,NULL,0,&count);if(!status&&count.bits>64)status=SILPH_MANAGED_CONTRACT;
    for(uint32_t i=0;!status&&i<count.bits;i++){
        SilphReadArgument index={.kind=SILPH_READ_I32,.value.bits=i};SilphReadValue kind={0};uint64_t amount=0;
        status=silph_read_call(read,list.reference,"get_Item",SILPH_READ_REFERENCE,&index,1,&item);
        if(!status)status=silph_read_is_type(read,item.reference,"Holoholo.Rpc","LootItemProto");
        if(!status)status=silph_read_call(read,item.reference,"get_TypeCase",SILPH_READ_ENUM32,NULL,0,&kind);
        if(!status&&kind.bits>16)status=SILPH_MANAGED_CONTRACT;
        if(!status)status=silph_reward_integer(read,item.reference,"Count",&amount);
        if(!status&&kind.bits==6)*xp+=amount;else if(!status&&kind.bits==2)*dust+=amount;
        int freed=silph_reward_drop(managed,&item.reference);if(!status)status=freed;
    }
    int a=silph_reward_drop(managed,&item.reference),b=silph_reward_drop(managed,&list.reference),c=silph_reward_drop(managed,&loot.reference);return status?status:a?a:b?b:c;
}
static inline int silph_quest_rewards(void *read,void *managed,SilphManagedRef result,uint64_t *xp,uint64_t *dust){
    SilphReadValue list={0},count={0};uint64_t a=0,b=0;int status=silph_read_is_type(read,result,"Holoholo.Rpc","CompleteQuestOutProto");
    if(!status)status=silph_read_field(read,result,"Rewards",SILPH_READ_REFERENCE,&list);
    if(!status)status=silph_read_call(read,list.reference,"get_Count",SILPH_READ_I32,NULL,0,&count);if(!status&&count.bits>32)status=SILPH_MANAGED_CONTRACT;
    for(uint32_t i=0;!status&&i<count.bits;i++){
        SilphReadValue reward={0},kind={0},amount={0};SilphReadArgument index={.kind=SILPH_READ_I32,.value.bits=i};
        status=silph_read_call(read,list.reference,"get_Item",SILPH_READ_REFERENCE,&index,1,&reward);
        if(!status)status=silph_read_is_type(read,reward.reference,"Holoholo.Rpc","QuestRewardProto");
        if(!status)status=silph_read_call(read,reward.reference,"get_RewardCase",SILPH_READ_ENUM32,NULL,0,&kind);
        if(!status&&kind.bits>24)status=SILPH_MANAGED_CONTRACT;
        if(!status&&(kind.bits==2||kind.bits==4)){
            status=silph_read_call(read,reward.reference,kind.bits==2?"get_Exp":"get_Stardust",SILPH_READ_I32,NULL,0,&amount);
            if(!status&&amount.bits>10000000)status=SILPH_MANAGED_CONTRACT;
            if(!status){if(kind.bits==2)a+=amount.bits;else b+=amount.bits;}
        }
        int freed=silph_reward_drop(managed,&reward.reference);if(!status)status=freed;
    }
    int freed=silph_reward_drop(managed,&list.reference);if(!status)status=freed;if(!status&&(a>10000000||b>10000000))status=SILPH_MANAGED_CONTRACT;
    if(!status){*xp=a;*dust=b;}return status;
}
static inline int silph_spin_rewards(void *read,void *managed,SilphManagedRef result,uint64_t *xp,uint64_t *dust){
    uint64_t a=0,b=0;int status=silph_reward_integer(read,result,"XpAwarded",&a);
    if(!status)status=silph_reward_loot(read,managed,result,"Loot",&a,&b);if(!status)status=silph_reward_loot(read,managed,result,"BonusLoot",&a,&b);
    if(!status&&(a>10000000||b>10000000))status=SILPH_MANAGED_CONTRACT;if(!status){*xp=a;*dust=b;}return status;
}
#endif
