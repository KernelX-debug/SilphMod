#ifndef SILPH_FARMER_SESSION_H
#define SILPH_FARMER_SESSION_H
#include "farmer-actions.h"
#include "maintenance-actions.h"
#include "research-actions.h"

enum {SILPH_SESSION_RUNNING=1,SILPH_SESSION_DONE=2,SILPH_SESSION_UNVERIFIED=3};
enum {SILPH_SESSION_CATCH_LIMIT=1,SILPH_SESSION_SPIN_LIMIT=2,SILPH_SESSION_RESPONSE_UNKNOWN=3};
typedef struct {
    SilphFarmerOptions farmer;
    uint32_t abi,catches,spins,clean,max_spins,limit_count,research;
    struct {int32_t item;uint32_t floor;} limits[64];
} SilphFarmerSessionOptions;
typedef struct {
    SilphFarmerMetrics farmer;
    uint64_t spins,spin_requests,deleted,delete_requests,spin_xp,callbacks,farmer_ns,maintenance_ns,max_farmer_ns,max_maintenance_ns,followups,refreshes;
    uint32_t state,reason,error,maintenance_state,spin_result;
    uint64_t spin_stardust;uint32_t reward_error;
    SilphResearchMetrics research;uint64_t research_ns,max_research_ns;
} SilphFarmerSessionMetrics;
int silph_farmer_session_validate(const SilphFarmerSessionOptions *);
size_t silph_farmer_session_size(void);
int silph_farmer_session_open(void *,size_t,void *,void *,void *,const SilphScheduleIdentity *,const SilphFarmerSessionOptions *,uint32_t);
bool silph_farmer_session_inventory_due(void *);
bool silph_farmer_session_idle(void *);
int silph_farmer_session_tick(void *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t,SilphFarmerSessionMetrics *);
int silph_farmer_session_close(void *);
#endif
