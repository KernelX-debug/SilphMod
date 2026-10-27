#ifndef SILPH_ROCKET_POLICY_H
#define SILPH_ROCKET_POLICY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { ROCKET_WAIT=0,ROCKET_FAST=1,ROCKET_CHARGED=2,ROCKET_SHIELD=3,ROCKET_SWAP=4,
    ROCKET_HEAL=5,ROCKET_REVIVE=6,ROCKET_HEALTHY=7,ROCKET_NO_ITEMS=8,ROCKET_INVALID=9 };
typedef struct {uint64_t id;int32_t hp,max_hp;bool known;} RocketHealth;
typedef struct {int32_t slot,move,energy_delta,duration;uint64_t ready_turn;bool disabled;} RocketMove;
typedef struct {
    bool bound,alive,paused,merged,started,finished,input_blocked;
    bool progress_ready;
    uint32_t ui_mode,shields,charge_slot,species;
    uint64_t generation,turn,serial,active,next_fast_turn;
    int32_t energy;
    float progress;
    uint64_t team[3];
    RocketHealth health[3];
    RocketMove moves[3];
} RocketBattleView;
typedef struct {
    uint32_t kind,event_type,oneof,attack_type;
    uint64_t outgoing,incoming;
    int32_t slot,item;
    float score;
} RocketDecision;
typedef struct {uint64_t generation,serial,turn,next_fast_turn;bool issued,pending,shield_window;} RocketDecisionGate;
typedef struct {uint64_t generation,serial,turn,active;uint32_t ui_mode;bool input_blocked;} RocketBattleClock;
typedef struct {uint64_t generation,turn,active,incoming,until;uint32_t type,ui_mode;int32_t energy;} RocketActionWatch;
typedef struct {int32_t potion,super_potion,hyper_potion,max_potion,revive,max_revive;} RocketCareItems;
bool rocket_battle_observe(RocketBattleClock *,RocketBattleView *);
bool rocket_action_observed(const RocketActionWatch *,const RocketBattleView *,uint64_t);
uint32_t rocket_battle_decide(const RocketBattleView *,RocketDecisionGate *,RocketDecision *);
bool rocket_battle_settled(RocketDecisionGate *,const RocketBattleView *);
uint32_t rocket_care_decide(const uint64_t[3],const RocketHealth[3],const RocketCareItems *,bool,RocketDecision *);
bool rocket_catch_accept(uint32_t,uint32_t,uint32_t,uint32_t,bool);
enum {ROCKET_RESULT_WAIT=0,ROCKET_RESULT_DISMISS=1,ROCKET_RESULT_EXPIRED=2};
typedef struct {uint64_t captured,until,next;uint32_t attempts;} RocketCaptureGate;
bool rocket_capture_watch(RocketCaptureGate *,uint64_t,uint64_t);
uint32_t rocket_capture_result(RocketCaptureGate *,uint64_t,bool,uint64_t);
#endif
