#define _GNU_SOURCE
#include "pilot-status.h"
#include "standalone/remote.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t number(const char *text,int base) {
    char *end=NULL;errno=0;uint64_t value=strtoull(text,&end,base);return !errno&&end&&!*end?value:0;
}
int main(int argc,char **argv) {
    if(argc!=5)return 2;
    pid_t pid=(pid_t)number(argv[1],10);uint64_t ticks=number(argv[2],10),address=number(argv[3],16),nonce=number(argv[4],10);
    char name[256];
    if(pid<=1||!ticks||!address||!nonce||silph_start_ticks(pid)!=ticks||silph_process_name(pid,name,sizeof(name))
        ||strcmp(name,"com.nianticlabs.pokemongo"))return 3;
    uint64_t values[SILPH_PILOT_WORDS]={0};
    if(silph_remote_read(pid,address,values,sizeof(values))||silph_start_ticks(pid)!=ticks)return 4;
    if(values[PILOT_MAGIC]!=SILPH_PILOT_MAGIC||values[PILOT_ABI]!=SILPH_PILOT_ABI||values[PILOT_PID]!=(uint64_t)pid
        ||values[PILOT_TICKS]!=ticks||values[PILOT_NONCE]!=nonce
#ifdef SILPH_PILOT_ACTIONS
#ifdef SILPH_PILOT_CONFIGURED
        ||values[PILOT_READONLY]>1
#else
        ||values[PILOT_READONLY]!=0
#endif
#else
        ||values[PILOT_READONLY]!=1
#endif
        )return 5;
    const char *keys[]={"magic","abi","pid","ticks","nonce","state","ownerTid","callbacks","jobs","cleanupCalls","created","released","remaining",
        "foreignThread","error","stopped","readOnly","resident","hookRemoved","pulses","busy","maxJobNs","mapVisible","schedulerState","queue","stopReason"
#ifdef SILPH_PILOT_GAME_READ
        ,"samples","owned","maxOwned","items","maxItems","wild","stops","candidatesRead","firstEncounterId","firstSpecies","accountHi","accountLo",
        "pokeBalls","greatBalls","ultraBalls","razzBerries","nanabBerries","pinapBerries","goldenRazzBerries","silverPinapBerries","readStage"
#endif
#ifdef SILPH_PILOT_EXTERNAL
        ,"controlKeyLo","controlAccepted","controlRejected","controlPulses","controlDisconnects","controlSequence"
#endif
#ifdef SILPH_PILOT_ACTIONS
        ,"encountersSent","throwsSent","catchesConfirmed","capturedId","targetId","freshCaptureResponse","farmerState","captureStatus","targetSpecies","actionError"
        ,"transfersSent","transfersConfirmed","transferDecision","transferReasons","transferError","captureScopeGranted"
#endif
#ifdef SILPH_PILOT_RPC_READ
        ,"rpcPrepared","protoValidated"
#endif
#ifdef SILPH_PILOT_KECLEON
        ,"kecleonState","kecleonVerified","kecleonShiny","kecleonCP","kecleonAttack","kecleonDefense","kecleonStamina","berriesUsed","rpcRequests"
#endif
#ifdef SILPH_PILOT_SPIN
        ,"spinStarted","spinsConfirmed","spinResult","spinRequests","spinXP"
        ,"itemsDiscarded","discardItem","discardBefore","discardAfter","discardRequested","discardFloor"
#endif
#ifdef SILPH_PILOT_COMBINED
        ,"sessionState","sessionReason","spinsConfirmed","spinRequests","itemsDiscarded","deleteRequests","spinXP","farmerNs","maintenanceNs","maxFarmerNs","maxMaintenanceNs","berriesUsed","berryRequests","followups","refreshes"
#ifdef SILPH_PILOT_SERVICE
        ,"researchScans","researchClaims","researchRemoved","researchEncounters","researchState","researchError","researchOperation","researchResult","researchNs","maxResearchNs","researchCatches","researchTransferred","researchRequests"
#endif
#endif
#ifdef SILPH_PILOT_SERVICE
        ,"locationState","locationError","locationStage","locationMethods","locationHooks","locationRemaining","locationCalls","locationChanged","locationLegacy","locationModern","locationExceptionBypass","locationObjectBypass","locationRefs","locationCleanup"
        ,"presentationAvailable","presentationFailed","presentationStage","scenesSkipped","eggsSkipped","giftsAccelerated","giftWaits","manualSelections","manualBatches","presentationHooks","presentationRefs","buddyState","buddyRequests","buddyResult","buddyError"
        ,"locationReadMethod","locationReadErrno","locationMemErrno","locationProtectErrno","locationRestoreErrno","locationHookIndex","locationCodeProtection"
        ,"actionsSkipped","promisePhase","promiseState","promiseReason","sceneOpenDrains","lastScene","scenesPending"
#ifdef SILPH_PILOT_COMBINED
        ,"farmerViewAvailable","farmerViewKind","farmerViewStage","farmerViewHooks","farmerViewRefs","farmerUpdates","farmerPumps","farmerMenuPumps","farmerViewSkipped","farmerViewMaxNs","farmerIntervalMs","farmerPhaseIntervalMs",
        "farmerViewName0","farmerViewName1","farmerViewName2","farmerViewName3","farmerViewName4","farmerViewName5","farmerViewName6","farmerViewName7",
        "readCacheHits","readCacheMisses"
#endif
#endif
    };
    _Static_assert(sizeof(keys)/sizeof(keys[0])==SILPH_PILOT_WORDS,"Pilot status fields disagree");
    printf("{\"event\":\"native_unity_pilot\",\"gameActions\":%" PRIu64 "",
#ifdef SILPH_PILOT_COMBINED
        values[PILOT_ACTION_ENCOUNTERS]+values[PILOT_ACTION_THROWS]+values[PILOT_ACTION_TRANSFER_REQUESTS]
        +values[PILOT_SESSION_BERRY_REQUESTS]+values[PILOT_SESSION_SPIN_REQUESTS]+values[PILOT_SESSION_DELETE_REQUESTS]
#ifdef SILPH_PILOT_SERVICE
        +values[PILOT_RESEARCH_REQUESTS]
#endif
#elif defined(SILPH_PILOT_KECLEON)
        values[PILOT_KECLEON_REQUESTS]
#elif defined(SILPH_PILOT_SPIN)
        values[PILOT_SPIN_REQUESTS]
#elif defined(SILPH_PILOT_ACTIONS)
        values[PILOT_ACTION_ENCOUNTERS]+values[PILOT_ACTION_THROWS]+values[PILOT_ACTION_TRANSFER_REQUESTS]
#else
        UINT64_C(0)
#endif
#ifdef SILPH_PILOT_SERVICE
        +values[PILOT_BUDDY_REQUESTS]
#endif
    );
    for(unsigned i=0;i<SILPH_PILOT_WORDS;i++){
#ifdef SILPH_PILOT_EXTERNAL
        if(i==PILOT_CONTROL_KEY_LO)continue;
#endif
        printf(",\"%s\":\"%" PRIu64 "\"",keys[i],values[i]);
    }
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_COMBINED)
    char view_name[65]={0};memcpy(view_name,&values[PILOT_VIEW_NAME0],64);for(unsigned i=0;i<64&&view_name[i];i++)if((unsigned char)view_name[i]<32||view_name[i]=='"'||view_name[i]=='\\')view_name[i]='?';
    printf(",\"farmerViewName\":\"%s\"",view_name);
#endif
    puts("}");return 0;
}
