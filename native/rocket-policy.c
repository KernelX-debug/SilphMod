#include "rocket-policy.h"
#include <limits.h>
#include <string.h>

bool rocket_catch_accept(uint32_t filters,uint32_t attack,uint32_t defense,uint32_t stamina,bool shiny){
    if(filters>31||attack>15||defense>15||stamina>15)return false;
    bool perfect=attack==15&&defense==15&&stamina==15;
    return !filters||((filters&1)&&perfect)||((filters&2)&&!attack&&!defense&&!stamina)
        ||((filters&4)&&shiny)||((filters&8)&&shiny&&perfect)||((filters&16)&&attack>=13&&defense>=13&&stamina>=13);
}

bool rocket_capture_watch(RocketCaptureGate *gate,uint64_t captured,uint64_t now){
    if(!gate||!captured||!now||now>UINT64_MAX-UINT64_C(60000000000))return false;
    *gate=(RocketCaptureGate){.captured=captured,.until=now+UINT64_C(60000000000),.next=now};return true;
}
uint32_t rocket_capture_result(RocketCaptureGate *gate,uint64_t current,bool post_encounter,uint64_t now){
    if(!gate||!gate->captured||!current||current!=gate->captured||!post_encounter)return ROCKET_RESULT_WAIT;
    if(now>=gate->until)return ROCKET_RESULT_EXPIRED;
    if(now<gate->next||gate->attempts>=3)return ROCKET_RESULT_WAIT;
    gate->attempts++;gate->next=now+UINT64_C(2000000000);return ROCKET_RESULT_DISMISS;
}

static bool valid_team(const uint64_t team[3]) {
    return team[0]&&team[1]&&team[2]&&team[0]!=team[1]&&team[0]!=team[2]&&team[1]!=team[2];
}
static int member(const uint64_t team[3],uint64_t id) {
    for(int i=0;i<3;i++)if(team[i]==id)return i;
    return -1;
}
static bool valid_health(const uint64_t team[3],const RocketHealth health[3]) {
    for(int i=0;i<3;i++)if(!health[i].known||health[i].id!=team[i]||health[i].hp<0||health[i].max_hp<=0
        ||health[i].hp>health[i].max_hp)return false;
    return true;
}
static uint32_t attack(RocketDecision *out,uint32_t kind,uint32_t type,int32_t slot,float score) {
    out->kind=kind;out->event_type=4;out->oneof=5;out->attack_type=type;out->slot=slot;out->score=score;
    return kind;
}
bool rocket_battle_observe(RocketBattleClock *clock,RocketBattleView *view){
    if(!clock||!view||!view->generation||!view->bound||!view->alive||!view->merged||!view->started||view->paused||view->finished)return false;
    if(clock->generation!=view->generation)*clock=(RocketBattleClock){.generation=view->generation};
    if(clock->serial&&view->turn<clock->turn)return false;
    if(!clock->serial||view->turn!=clock->turn||view->active!=clock->active||view->ui_mode!=clock->ui_mode||view->input_blocked!=clock->input_blocked){
        if(clock->serial==UINT64_MAX)return false;
        clock->serial++;clock->turn=view->turn;clock->active=view->active;clock->ui_mode=view->ui_mode;clock->input_blocked=view->input_blocked;
    }
    view->serial=clock->serial;return true;
}
bool rocket_action_observed(const RocketActionWatch *watch,const RocketBattleView *view,uint64_t now){
    if(!watch||!view||!watch->generation||watch->generation!=view->generation||!watch->active||!view->active
        ||(watch->type!=1&&watch->type!=2&&watch->type!=3&&watch->type!=4&&watch->type!=7)
        ||!valid_team(view->team)||member(view->team,view->active)<0||!view->bound||!view->alive||!view->merged||!view->started||view->paused||view->finished||view->turn<watch->turn)return false;
    if(view->active!=watch->active)return true;
    if(watch->type==2||watch->type==3){
        if(watch->ui_mode==0&&view->ui_mode==5)return true;
        if(watch->ui_mode==5&&view->ui_mode!=5)return true;
        return watch->ui_mode==0&&view->turn>watch->turn&&view->energy<watch->energy;
    }
    return watch->type==1&&((watch->ui_mode==0&&view->ui_mode!=0)||(now>watch->until&&view->turn>watch->turn));
}
bool rocket_battle_settled(RocketDecisionGate *gate,const RocketBattleView *view) {
    if(!gate||!view||!view->bound||!view->alive||!view->merged||view->paused||!gate->pending
        ||gate->generation!=view->generation
        ||view->serial<gate->serial||view->turn<gate->turn||(view->serial==gate->serial&&view->turn==gate->turn))return false;
    gate->pending=false;return true;
}
uint32_t rocket_battle_decide(const RocketBattleView *view,RocketDecisionGate *gate,RocketDecision *out) {
    if(!out)return ROCKET_INVALID;
    memset(out,0,sizeof(*out));
    if(!view||!gate||!view->generation||!valid_team(view->team))return out->kind=ROCKET_INVALID;
    if(gate->issued&&gate->generation!=view->generation)return out->kind=ROCKET_INVALID;
    if(!view->bound||!view->alive||view->paused||!view->merged||!view->started||view->finished)return ROCKET_WAIT;
    if(!valid_health(view->team,view->health)||view->shields>2||view->energy<0)return out->kind=ROCKET_INVALID;
    const int active=member(view->team,view->active);
    if(active<0)return out->kind=ROCKET_INVALID;
    if(view->ui_mode!=6)gate->shield_window=false;
    if(gate->pending)return ROCKET_WAIT;
    if(gate->issued&&(view->serial<gate->serial||view->turn<gate->turn
        ||(view->serial==gate->serial&&view->turn==gate->turn)))return ROCKET_WAIT;
    if(view->input_blocked)return ROCKET_WAIT;
    if(view->ui_mode==6&&view->shields&&!gate->shield_window){attack(out,ROCKET_SHIELD,4,4,1.0f);gate->shield_window=true;}
    else if(view->ui_mode==8||view->ui_mode==10) {
        if(view->health[active].hp>0)return ROCKET_WAIT;
        for(int i=0;i<3;i++)if(i!=active&&view->health[i].hp>0) {
            out->kind=ROCKET_SWAP;out->event_type=7;out->oneof=8;
            out->outgoing=view->active;out->incoming=view->team[i];break;
        }
    } else if(view->ui_mode==5&&view->progress_ready) {
        if((view->charge_slot!=2&&view->charge_slot!=3)||!(view->progress>=0.0f&&view->progress<=1.0f))return out->kind=ROCKET_INVALID;
        attack(out,ROCKET_CHARGED,view->charge_slot,(int32_t)view->charge_slot,view->progress);
    } else if(view->ui_mode==0&&view->health[active].hp>0) {
        for(int i=0;i<3;i++) {
            const RocketMove *move=&view->moves[i];
            if((move->slot!=2&&move->slot!=3)||move->disabled||move->ready_turn>view->turn||move->move<=0
                ||move->energy_delta>=0||move->energy_delta==INT_MIN)continue;
            if(view->energy>=-move->energy_delta){attack(out,ROCKET_CHARGED,(uint32_t)move->slot,move->slot,1.0f);break;}
        }
        if(out->kind==ROCKET_WAIT&&view->next_fast_turn<=view->turn&&gate->next_fast_turn<=view->turn)for(int i=0;i<3;i++) {
            const RocketMove *move=&view->moves[i];
            if(move->slot==1&&move->move>0&&move->duration>=0&&!move->disabled&&move->ready_turn<=view->turn) {
                uint64_t duration=(uint64_t)move->duration+1;
                if(view->turn>UINT64_MAX-duration)return out->kind=ROCKET_INVALID;
                attack(out,ROCKET_FAST,1,1,0.0f);gate->next_fast_turn=view->turn+duration;break;
            }
        }
    }
    if(out->kind!=ROCKET_WAIT){gate->generation=view->generation;gate->serial=view->serial;gate->turn=view->turn;gate->issued=true;gate->pending=true;}
    return out->kind;
}
uint32_t rocket_care_decide(const uint64_t team[3],const RocketHealth health[3],const RocketCareItems *items,
    bool fresh,RocketDecision *out) {
    if(!out)return ROCKET_INVALID;
    memset(out,0,sizeof(*out));
    if(!team||!health||!items||!valid_team(team))return out->kind=ROCKET_INVALID;
    if(!fresh)return ROCKET_WAIT;
    if(!valid_health(team,health))return out->kind=ROCKET_INVALID;
    if(items->potion<0||items->super_potion<0||items->hyper_potion<0||items->max_potion<0
        ||items->revive<0||items->max_revive<0)return out->kind=ROCKET_INVALID;
    for(int i=0;i<3;i++)if(health[i].hp==0) {
        out->incoming=team[i];out->item=items->max_revive?202:items->revive?201:0;
        return out->kind=out->item?ROCKET_REVIVE:ROCKET_NO_ITEMS;
    }
    for(int i=0;i<3;i++)if(health[i].hp<health[i].max_hp) {
        const int32_t missing=health[i].max_hp-health[i].hp;
        const int32_t count[]={items->potion,items->super_potion,items->hyper_potion,items->max_potion};
        const int32_t amount[]={20,50,200,INT_MAX};
        int selected=-1;
        for(int j=0;j<4;j++)if(count[j]){selected=j;if(amount[j]>=missing)break;}
        out->incoming=team[i];out->item=selected<0?0:101+selected;
        return out->kind=out->item?ROCKET_HEAL:ROCKET_NO_ITEMS;
    }
    return out->kind=ROCKET_HEALTHY;
}
uint32_t rocket_care_ordered(const uint64_t team[3],const RocketHealth health[3],const RocketCareItems *items,const RocketCareOrder *order,bool fresh,RocketDecision *out){
    uint32_t kind=rocket_care_decide(team,health,items,fresh,out);
    if(!order||(kind!=ROCKET_HEAL&&kind!=ROCKET_REVIVE))return kind;
    const uint32_t *ids=kind==ROCKET_HEAL?order->potions:order->revives;unsigned count=kind==ROCKET_HEAL?4:2;uint32_t first=kind==ROCKET_HEAL?101:201;
    bool custom=false;for(unsigned i=0;i<count;i++)custom|=ids[i]!=0;if(!custom)return kind;
    const int32_t counts[]={items->potion,items->super_potion,items->hyper_potion,items->max_potion,items->revive,items->max_revive};
    for(unsigned i=0;i<count;i++){if(ids[i]<first||ids[i]>=first+count)return out->kind=ROCKET_INVALID;for(unsigned j=0;j<i;j++)if(ids[i]==ids[j])return out->kind=ROCKET_INVALID;}
    for(unsigned i=0;i<count;i++)if(counts[ids[i]-first+(kind==ROCKET_REVIVE?4:0)]>0){out->item=(int32_t)ids[i];return kind;}
    return out->kind=ROCKET_NO_ITEMS;
}
