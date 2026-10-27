#ifndef SILPH_ROCKET_BRIDGE_H
#define SILPH_ROCKET_BRIDGE_H
#include "managed-action.h"
enum {RK_TOP=1,RK_TAP=2,RK_CHALLENGE=3,RK_SUBMIT=4,RK_RESOLVE=5,RK_GEM=6,RK_START_ENCOUNTER=7,RK_STOP_INCIDENT=8,RK_ENCOUNTER_FINISH=9,RK_CAPTURE_COMPLETED=10,RK_DIALOG=11,RK_STOP_EXIT=12,RK_PARTY=13,RK_LAYER=14,RK_PARTY_REFRESH=15,RK_MAP_REFRESH=16,RK_CAPTURE_INFO=17,RK_SHIELD=18,RK_CANCEL=19,RK_PARTY_CANCEL=20,RK_RESULT_CLOSE=21,RK_INCIDENT_CLOSE=22,RK_PARTY_RECOMMEND=23,RK_FAST_GUI=24,RK_CAPTURE_RECORD=25,RK_PLAYER_ALIAS=26,RK_REWARD_DISMISS=27,RK_REWARD_COMPLETE=28};
int silph_rocket_invoke(void *,const SilphSchedulePermit *,uint32_t,SilphManagedRef,const SilphReadArgument *,uint32_t,SilphReadValue *);
int silph_rocket_attack(void *,const SilphSchedulePermit *,SilphManagedRef,uint32_t,float,uint64_t,uint64_t,SilphManagedRef *);
int silph_rocket_class(void *,SilphManagedRef,char *,uint32_t);
int silph_rocket_medicine(void *,const SilphSchedulePermit *,SilphManagedRef,uint64_t,uint32_t,SilphManagedRef *);
int silph_rocket_prepared_team(void *,SilphManagedRef,uint64_t[3],bool *);
int silph_rocket_map_settings(void *,const SilphSchedulePermit *,SilphManagedRef,float[3],bool);
#endif
