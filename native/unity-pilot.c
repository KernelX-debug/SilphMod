#define _GNU_SOURCE
#define SILPH_MAP_ENTRY silph_native_map_contract_probe
#ifdef SILPH_PILOT_SERVICE
#define silph_native_bootstrap silph_native_baseline_bootstrap
#endif
#include "map-observer.c"
#ifdef SILPH_PILOT_SERVICE
#undef silph_native_bootstrap
#endif
#include "scheduler.h"
#include "pilot-status.h"
#include <shadowhook.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <time.h>
#include "startup-thread.h"
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
#include "farmer-view.h"
static void *farmer_view;
static unsigned farmer_view_attempted;
static atomic_uint farmer_continuation;
static uint32_t farmer_view_kind;
static SilphFarmerViewMetrics farmer_view_metrics;
static void farmer_pump(void *,bool,uint32_t);
static _Atomic uint64_t progress[SILPH_PILOT_WORDS];
static void farmer_view_progress(void) {
    if(farmer_view)silph_farmer_view_metrics(farmer_view,&farmer_view_metrics);
    atomic_store(&progress[PILOT_VIEW_AVAILABLE],farmer_view_metrics.available);
    atomic_store(&progress[PILOT_VIEW_KIND],farmer_view_metrics.kind);atomic_store(&progress[PILOT_VIEW_STAGE],farmer_view_metrics.stage);
    atomic_store(&progress[PILOT_VIEW_HOOKS],farmer_view_metrics.hooks);atomic_store(&progress[PILOT_VIEW_REFS],farmer_view_metrics.remaining);
    atomic_store(&progress[PILOT_VIEW_UPDATES],farmer_view_metrics.updates);atomic_store(&progress[PILOT_VIEW_PUMPS],farmer_view_metrics.pumps);
    atomic_store(&progress[PILOT_VIEW_MENUS],farmer_view_metrics.menu_pumps);atomic_store(&progress[PILOT_VIEW_SKIPPED],farmer_view_metrics.skipped);
    atomic_store(&progress[PILOT_VIEW_MAX_NS],farmer_view_metrics.max_ns);
    for(unsigned i=0;i<8;i++){uint64_t word=0;memcpy(&word,farmer_view_metrics.name+i*8,8);atomic_store(&progress[PILOT_VIEW_NAME0+i],word);}
}
#endif
#ifdef SILPH_PILOT_CONFIGURED
#include "session-link.h"
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_KECLEON)
#include "kecleon-session.h"
static unsigned kec_session_started;
static SilphKecleonSessionMetrics kec_session;
#endif
static void *session_link;
static SilphSessionConfig session_config;
static SilphSessionSnapshot session_snapshot;
static unsigned session_configured;
static uint64_t publication_sequence;
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_COMBINED)
#include "research-actions.h"
static uint64_t session_xp,session_dust,session_fled;
static uint32_t session_reward_error;
static SilphResearchMetrics session_research;
#endif
#endif
#ifdef SILPH_PILOT_RPC_READ
#include "managed-action.h"
#include "managed-proto.h"
#endif
#ifdef SILPH_PILOT_ACTIONS
#include "farmer-actions.h"
#ifdef SILPH_PILOT_KECLEON
#include "kecleon-actions.h"
#endif
#ifdef SILPH_PILOT_SPIN
#include "maintenance-actions.h"
#endif
#ifdef SILPH_PILOT_COMBINED
#include "farmer-session.h"
static unsigned session_started;
#endif
static void *farmer_state;
static unsigned farmer_started;
#endif
#ifdef SILPH_PILOT_EXTERNAL
#include "control.h"
static void *control_state;
static uint64_t control_key_lo;
#endif
#ifdef SILPH_PILOT_GAME_READ
#include "game-read.h"
static void *read_state,*game_state,*active_map;
static unsigned read_started,game_started;
static Image game_image;
#endif

static _Atomic uint64_t progress[SILPH_PILOT_WORDS];
static atomic_uint enabled,owner;
static SilphManagedApi managed_api;
static MapContract map;
static void *schedule,*managed_state,*hook_stub,*self_pin,*backend_pin;
static SilphScheduleLease lease;
static SilphScheduleConfig config;
static uint64_t now_ns(void);
#ifdef SILPH_PILOT_SERVICE
#include "visual-tools.h"
#include "location-tools.h"
#include "presentation-tools.h"
#include "buddy-actions.h"
#include "recovery-policy.h"
#include "gift-actions.h"
static void *gift_state;
static unsigned gift_attempted;
static SilphGiftMetrics gift_metrics;
static void *presentation_state,*buddy_state;
static unsigned presentation_attempted,buddy_attempted;
static SilphPresentationMetrics presentation_metrics;
static SilphBuddyMetrics buddy_metrics;
static SilphRecovery recovery;
static uint32_t auxiliary_reason;
#ifdef SILPH_PILOT_COMBINED
static uint64_t recovery_due;
static SilphFarmerSessionMetrics previous_session;
#endif
static void presentation_progress(void){
    if(presentation_state)silph_presentation_metrics(presentation_state,&presentation_metrics);
    atomic_store(&progress[PILOT_PRESENT_AVAILABLE],presentation_metrics.available);atomic_store(&progress[PILOT_PRESENT_FAILED],presentation_metrics.failed);atomic_store(&progress[PILOT_PRESENT_STAGE],presentation_metrics.stage);
    atomic_store(&progress[PILOT_SCENES],presentation_metrics.scenes);atomic_store(&progress[PILOT_EGGS],presentation_metrics.eggs);atomic_store(&progress[PILOT_GIFTS],presentation_metrics.gifts);atomic_store(&progress[PILOT_GIFT_WAITS],presentation_metrics.waits);atomic_store(&progress[PILOT_SELECTIONS],presentation_metrics.selections);atomic_store(&progress[PILOT_BATCHES],presentation_metrics.batches);atomic_store(&progress[PILOT_PRESENT_HOOKS],presentation_metrics.remaining_hooks);atomic_store(&progress[PILOT_PRESENT_REFS],presentation_metrics.remaining);
    atomic_store(&progress[PILOT_BUDDY_STATE],buddy_metrics.state);atomic_store(&progress[PILOT_BUDDY_REQUESTS],buddy_metrics.requests);atomic_store(&progress[PILOT_BUDDY_RESULT],buddy_metrics.result);atomic_store(&progress[PILOT_BUDDY_ERROR],buddy_metrics.error);
    atomic_store(&progress[PILOT_SCENE_OPEN_DRAINS],presentation_metrics.open_drains);atomic_store(&progress[PILOT_SCENE_LAST],presentation_metrics.last_scene);atomic_store(&progress[PILOT_SCENE_PENDING],presentation_metrics.pending);
    atomic_store(&progress[PILOT_SCENE_SUPPORTED],presentation_metrics.scene_supported);atomic_store(&progress[PILOT_SCENE_ERRORS],presentation_metrics.scene_errors);atomic_store(&progress[PILOT_SCENE_RETRIES],presentation_metrics.scene_retries);atomic_store(&progress[PILOT_SCENE_TIMEOUTS],presentation_metrics.scene_timeouts);
}
static atomic_uint location_attempted;
static atomic_uint location_bootstrap_state;
static _Atomic uint64_t location_bootstrap_until;
static bool location_allowed(void *unused){
    (void)unused;
    if(atomic_load(&location_bootstrap_state)==2){uint64_t until=atomic_load(&location_bootstrap_until);return until&&now_ns()<until;}
    return atomic_load(&enabled)&&silph_schedule_session_alive(schedule,lease);
}
static int location_symbol(struct dl_phdr_info *info,size_t bytes,void *out){
    (void)bytes;const char *name=strrchr(info->dlpi_name,'/');name=name?name+1:info->dlpi_name;
    if(strcmp(name,"libart.so")&&strcmp(name,"libnativehelper.so"))return 0;Image image={0};if(!image_from(info,&image))return 0;
    void *symbol=export_address(&image,"JNI_GetCreatedJavaVMs");if(!symbol)return 0;*(void **)out=symbol;return 1;
}
static void location_progress(void){
    SilphLocationMetrics location_metrics;
    silph_location_metrics(&location_metrics);
    atomic_store(&progress[PILOT_LOCATION_STATE],location_metrics.state);atomic_store(&progress[PILOT_LOCATION_ERROR],location_metrics.error);atomic_store(&progress[PILOT_LOCATION_STAGE],location_metrics.stage);
    atomic_store(&progress[PILOT_LOCATION_METHODS],location_metrics.methods);atomic_store(&progress[PILOT_LOCATION_HOOKS],location_metrics.hooks);atomic_store(&progress[PILOT_LOCATION_REMAINING],location_metrics.remaining);
    atomic_store(&progress[PILOT_LOCATION_CALLS],location_metrics.calls);atomic_store(&progress[PILOT_LOCATION_CHANGED],location_metrics.changed);atomic_store(&progress[PILOT_LOCATION_LEGACY],location_metrics.legacy_calls);atomic_store(&progress[PILOT_LOCATION_MODERN],location_metrics.modern_calls);
    atomic_store(&progress[PILOT_LOCATION_EXCEPTIONS],location_metrics.exception_bypass);atomic_store(&progress[PILOT_LOCATION_OBJECTS],location_metrics.object_bypass);atomic_store(&progress[PILOT_LOCATION_REFS],location_metrics.global_refs);atomic_store(&progress[PILOT_LOCATION_CLEANUP],location_metrics.cleanup);
    atomic_store(&progress[PILOT_LOCATION_READ_METHOD],location_metrics.read_method);atomic_store(&progress[PILOT_LOCATION_READ_ERRNO],location_metrics.read_errno);atomic_store(&progress[PILOT_LOCATION_MEM_ERRNO],location_metrics.mem_errno);
    atomic_store(&progress[PILOT_LOCATION_PROTECT_ERRNO],location_metrics.protect_errno);atomic_store(&progress[PILOT_LOCATION_RESTORE_ERRNO],location_metrics.restore_errno);atomic_store(&progress[PILOT_LOCATION_HOOK_INDEX],location_metrics.hook_index);atomic_store(&progress[PILOT_LOCATION_CODE_PROTECTION],location_metrics.code_protection);
}
static void *location_bootstrap_worker(void *unused){
    (void)unused;void *get_vms=NULL;dl_iterate_phdr(location_symbol,&get_vms);
    atomic_store(&location_attempted,1);int installed=silph_location_open(get_vms,location_allowed,NULL);location_progress();
    if(installed){atomic_store(&progress[PILOT_ERROR],1700+installed);atomic_store(&location_bootstrap_state,3);return NULL;}
    atomic_store(&location_bootstrap_state,2);
    while(atomic_load(&location_bootstrap_state)==2&&now_ns()<atomic_load(&location_bootstrap_until)){struct timespec pause={0,10000000};nanosleep(&pause,NULL);}
    unsigned expected=2;if(atomic_compare_exchange_strong(&location_bootstrap_state,&expected,5)){
        int closed=silph_location_close();location_progress();atomic_store(&progress[PILOT_ERROR],closed?1800+closed:1740);
    }
    return NULL;
}
__attribute__((visibility("default")))
int silph_native_bootstrap(SilphNativeReport *report,uint32_t capacity){
    int status=silph_native_baseline_bootstrap(report,capacity);if(status)return status;
    unsigned expected=0;if(atomic_load(&progress[PILOT_MAGIC])||!atomic_compare_exchange_strong(&location_bootstrap_state,&expected,1))return 41;
    Dl_info info;char backend[4096];
    if(!dladdr((void *)silph_native_bootstrap,&info)||!info.dli_fname)goto failed;
    const char *slash=strrchr(info.dli_fname,'/');
    if(!slash||snprintf(backend,sizeof(backend),"%.*s/libshadowhook.so",(int)(slash-info.dli_fname),info.dli_fname)>=(int)sizeof(backend))goto failed;
    self_pin=dlopen(info.dli_fname,RTLD_NOW|RTLD_NODELETE);backend_pin=dlopen(backend,RTLD_NOW|RTLD_NODELETE);
    if(!self_pin||!backend_pin||shadowhook_init(SHADOWHOOK_MODE_UNIQUE,true))goto failed;
    atomic_store(&location_bootstrap_until,now_ns()+UINT64_C(120000000000));
    pthread_t thread;if(pthread_create(&thread,NULL,location_bootstrap_worker,NULL))goto failed;pthread_detach(thread);
    snprintf(report->detail,sizeof(report->detail),"Native location bootstrap: pending deadlineMs=120000 gameActions=0");return 0;
failed:
    atomic_store(&location_bootstrap_state,3);return 42;
}
static void *visual_state;
static unsigned visual_attempted;
static SilphVisualMetrics visual_metrics;
static bool visual_allowed(void *unused){(void)unused;return atomic_load(&enabled)&&silph_schedule_visual_allowed(schedule,lease);}
static bool visual_address(uintptr_t address,void *image){return accessible(image,address,16,PF_X);}
static void prepare_tools(void){
    if(!session_configured){
        if(silph_session_link_config(session_link,&session_config))return;
        session_configured=1;atomic_store(&progress[PILOT_READONLY],!!(session_config.flags&SILPH_CFG_READONLY));
    }
    if(session_config.flags&SILPH_CFG_READONLY)return;
    if(!visual_attempted){
        visual_attempted=1;
        if(session_config.flags&(SILPH_CFG_TRAINER_NAME|SILPH_CFG_IV_NAMES)){
            visual_state=calloc(1,silph_visual_size());
            if(!visual_state||silph_visual_open(visual_state,silph_visual_size(),&managed_api,resolve_managed,&game_image,map.pointer_offset,session_config.flags,visual_allowed,NULL,visual_address)){
                free(visual_state);visual_state=NULL;visual_metrics.failed=session_config.flags&(SILPH_CFG_TRAINER_NAME|SILPH_CFG_IV_NAMES);
            }
        }
    }
    if(!presentation_attempted){
        presentation_attempted=1;uint32_t flags=session_config.flags&(SILPH_CFG_SCENES|SILPH_CFG_GIFTS|SILPH_CFG_MANUAL);
        if(flags){presentation_state=calloc(1,silph_presentation_size());if(!presentation_state||silph_presentation_open(presentation_state,silph_presentation_size(),&managed_api,resolve_managed,&game_image,map.pointer_offset,flags,visual_allowed,NULL,visual_address)){free(presentation_state);presentation_state=NULL;presentation_metrics.failed=flags;}}
    }
}
#ifdef SILPH_PILOT_COMBINED
static uint64_t view_retry_at;
static void prepare_view(void *map_owner){
    if(farmer_view_attempted||now_ns()<view_retry_at||(session_configured&&(session_config.flags&SILPH_CFG_READONLY)))return;
    view_retry_at=now_ns()+UINT64_C(1000000000);farmer_view=calloc(1,silph_farmer_view_size());if(!farmer_view)return;
    int status=silph_farmer_view_open(farmer_view,silph_farmer_view_size(),&managed_api,resolve_managed,&game_image,map.pointer_offset,map_owner,visual_allowed,NULL,visual_address,farmer_pump);
    if(!status)farmer_view_attempted=1;
    else if(!silph_farmer_view_close(farmer_view,&farmer_view_metrics)){free(farmer_view);farmer_view=NULL;}
}
#endif
#endif
#ifndef SILPH_PILOT_GAME_READ
static SilphManagedRef text;
static SilphManagedMethod getter;
#endif
static bool (*original)(void *,const void *);
static unsigned char original_bytes[16];
static uint64_t now_ns(void) {
    struct timespec value;if(clock_gettime(CLOCK_MONOTONIC,&value))return 0;
    return(uint64_t)value.tv_sec*UINT64_C(1000000000)+(uint64_t)value.tv_nsec;
}
static uint64_t own_ticks(void) {
    FILE *file=fopen("/proc/self/stat","re");if(!file)return 0;
    char line[4096];char *read=fgets(line,sizeof(line),file);fclose(file);if(!read)return 0;
    char *at=strrchr(line,')');if(!at)return 0;at+=2;
    for(unsigned field=3;field<22;field++){at=strchr(at,' ');if(!at)return 0;at++;}
    return strtoull(at,NULL,10);
}
static int execute(void *unused,const SilphScheduleJob *job,const SilphSchedulePermit *permit) {
    (void)unused;
#if defined(SILPH_PILOT_ACTIONS) && !defined(SILPH_PILOT_READONLY_ONLY)
    if(job->kind!=SILPH_JOB_WRITE||!silph_schedule_permitted(permit,true))return 1;
#else
    if(job->kind!=SILPH_JOB_OBSERVE||!silph_schedule_permitted(permit,false)||silph_schedule_permitted(permit,true))return 1;
#endif
    int status=0;
#ifdef SILPH_PILOT_CONFIGURED
    if(!session_configured){
        status=silph_session_link_config(session_link,&session_config);
        if(status==SILPH_SCHEDULE_BUSY||status==SILPH_SCHEDULE_STATE){atomic_fetch_add(&progress[PILOT_JOBS],1);return 0;}
        if(status)return 1;session_configured=1;atomic_store(&progress[PILOT_READONLY],!!(session_config.flags&SILPH_CFG_READONLY));
    }
#if defined(SILPH_PILOT_KECLEON) && !defined(SILPH_PILOT_SERVICE)
    if(!(session_config.flags&SILPH_CFG_READONLY)){atomic_store(&progress[PILOT_ERROR],1401);return 1;}
#endif
#endif
#ifdef SILPH_PILOT_GAME_READ
    void *map_owner=active_map;
#endif
#ifdef SILPH_PILOT_SERVICE
    location_progress();
#endif
    if(!managed_state) {
        if(!managed_api.thread_current())return 2;
        size_t bytes=silph_managed_size(64);managed_state=calloc(1,bytes);if(!managed_state)return 3;
        status=silph_managed_open(managed_state,bytes,&managed_api,64,64);
        if(status){free(managed_state);managed_state=NULL;return status;}
        SilphManagedMetrics m;status=silph_managed_metrics(managed_state,&m);
        if(status||m.owns_attachment)return 4;
#ifdef SILPH_PILOT_GAME_READ
        read_state=calloc(1,silph_read_size());game_state=calloc(1,silph_game_read_size());
        if(!read_state||!game_state)status=3;
        if(!status)status=silph_read_open(read_state,silph_read_size(),managed_state,&managed_api,resolve_managed,&game_image);
        if(!status)read_started=1;
        if(!status)status=silph_game_read_open(game_state,silph_game_read_size(),read_state,&managed_api,managed_state,map_owner);
        if(!status)game_started=1;
#ifdef SILPH_PILOT_ACTIONS
        if(!status)status=silph_game_read_compact(game_state);
#ifdef SILPH_PILOT_CONFIGURED
        if(!(session_config.flags&SILPH_CFG_READONLY)){
#endif
        if(!status)status=silph_action_enable(read_state,resolve_managed,&game_image);
#ifdef SILPH_PILOT_KECLEON
        if(!status)status=silph_proto_enable(read_state,resolve_managed,&game_image);
        if(!status)status=silph_rpc_enable(read_state,resolve_managed,&game_image);
#ifdef SILPH_PILOT_SERVICE
        if(!status){farmer_state=calloc(1,silph_kecleon_session_size());if(!farmer_state)status=SILPH_MANAGED_CAPACITY;}
#else
        if(!status){farmer_state=calloc(1,silph_kecleon_actions_size());if(!farmer_state)status=SILPH_MANAGED_CAPACITY;}
        SilphScheduleIdentity real=config.identity;real.account_hi=real.account_lo=0;
        SilphKecleonOptions options={.abi=1,.filters=0,.quality=4,.curve=1,.use_berries=1,.berry_count=3,.max_throws=3,.berries={701,705,703}};
        if(!status)status=silph_kecleon_actions_open(farmer_state,silph_kecleon_actions_size(),read_state,managed_state,game_state,&real,&options);
#endif
#elif defined(SILPH_PILOT_SPIN)
        if(!status)status=silph_proto_enable(read_state,resolve_managed,&game_image);
        if(!status)status=silph_rpc_enable(read_state,resolve_managed,&game_image);
        if(!status){farmer_state=calloc(1,silph_maintenance_size());if(!farmer_state)status=SILPH_MANAGED_CAPACITY;}
#elif defined(SILPH_PILOT_COMBINED)
        if(!status)status=silph_proto_enable(read_state,resolve_managed,&game_image);
        if(!status)status=silph_rpc_enable(read_state,resolve_managed,&game_image);
        if(!status){farmer_state=calloc(1,silph_farmer_session_size());if(!farmer_state)status=SILPH_MANAGED_CAPACITY;}
#ifdef SILPH_PILOT_SERVICE
        if(!status)silph_research_enable(read_state,resolve_managed,&game_image);
#endif
#else
        if(!status){farmer_state=calloc(1,silph_farmer_actions_size());if(!farmer_state)status=SILPH_MANAGED_CAPACITY;}
        SilphScheduleIdentity real=config.identity;real.account_hi=real.account_lo=0;
        if(!status)status=silph_farmer_actions_open(farmer_state,silph_farmer_actions_size(),read_state,managed_state,game_state,&real,
#ifdef SILPH_PILOT_TRANSFER
            1
#else
            0
#endif
        );
#ifdef SILPH_PILOT_CONTINUOUS
        SilphFarmerOptions options={.abi=1,.continuous=1,.max_catches=3,.quality=4,.curve=1,.use_berries=1,.keep_options=127,.berry_count=3,.berries={701,705,703}};
        if(!status)status=silph_farmer_actions_configure(farmer_state,&options);
#endif
#endif
        if(!status)farmer_started=1;
#ifdef SILPH_PILOT_CONFIGURED
        }
#endif
#endif
#else
        status=silph_managed_ascii(managed_state,"SilphMod native",15,&text);
        if(!status)status=silph_managed_getter_i32(managed_state,text,"get_Length",&getter);
#endif
    }
#ifdef SILPH_PILOT_GAME_READ
    SilphGameSnapshot snapshot={0};
    bool inventory=true;
#if defined(SILPH_PILOT_KECLEON)
#ifdef SILPH_PILOT_SERVICE
    inventory=silph_kecleon_session_inventory_due(farmer_state);
#else
    inventory=silph_kecleon_inventory_due(farmer_state);
#endif
#elif defined(SILPH_PILOT_COMBINED)
    inventory=silph_farmer_session_inventory_due(farmer_state);
#elif defined(SILPH_PILOT_ACTIONS) && !defined(SILPH_PILOT_SPIN)
    inventory=silph_farmer_actions_inventory_due(farmer_state);
#endif
    if(!status)status=silph_game_read_poll(game_state,map_owner,inventory,&snapshot);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    SilphReadCacheMetrics cache_metrics={0};if(read_state&&!silph_read_cache_metrics(read_state,&cache_metrics)){
        atomic_store(&progress[PILOT_READ_CACHE_HITS],cache_metrics.hits);atomic_store(&progress[PILOT_READ_CACHE_MISSES],cache_metrics.misses);
    }
    if(!status)prepare_view(map_owner);
    if(farmer_view){silph_farmer_view_interval(farmer_view,session_config.delay_ms);atomic_store(&progress[PILOT_VIEW_INTERVAL],session_config.delay_ms);farmer_view_progress();}
#endif
    if(game_state)atomic_store(&progress[PILOT_READ_STAGE],silph_game_read_stage(game_state));
    if(status){atomic_store(&progress[PILOT_ERROR],100+status);return 1;}
#ifdef SILPH_PILOT_SERVICE
    prepare_tools();
    if(visual_state)silph_visual_metrics(visual_state,&visual_metrics);
    bool auxiliary_wait=false;
    if((session_config.flags&SILPH_CFG_BUDDY)&&!(session_config.flags&SILPH_CFG_READONLY)){
        SilphScheduleIdentity identity=config.identity;identity.account_hi=snapshot.account_hi;identity.account_lo=snapshot.account_lo;
        if(!buddy_attempted){buddy_attempted=1;status=silph_proto_enable(read_state,resolve_managed,&game_image);if(!status)status=silph_rpc_enable(read_state,resolve_managed,&game_image);if(!status){buddy_state=calloc(1,silph_buddy_size());if(!buddy_state)status=SILPH_MANAGED_CAPACITY;}if(!status)status=silph_buddy_open(buddy_state,silph_buddy_size(),read_state,managed_state,game_state,&identity);}
        if(!status)status=silph_buddy_tick(buddy_state,permit,now_ns(),&buddy_metrics);
        if(status){atomic_store(&progress[PILOT_ERROR],1900+status);return 1;}
        auxiliary_wait=buddy_metrics.state>=SILPH_BUDDY_CHECK&&buddy_metrics.state<=SILPH_BUDDY_REMOVING;
        auxiliary_reason=auxiliary_wait?(buddy_metrics.state<=SILPH_BUDDY_CHECKING?12:13):buddy_metrics.state==SILPH_BUDDY_REMOVED?11:0;
        if(auxiliary_reason==11){auxiliary_wait=true;silph_schedule_stop(schedule,lease);}
    }
    presentation_progress();
    if((session_config.flags&(SILPH_CFG_OPEN_GIFTS|SILPH_CFG_SEND_GIFTS))&&!(session_config.flags&SILPH_CFG_READONLY)&&!auxiliary_wait){
        if(!gift_attempted){gift_attempted=1;gift_state=calloc(1,silph_gift_size());SilphScheduleIdentity identity=config.identity;identity.account_hi=snapshot.account_hi;identity.account_lo=snapshot.account_lo;
            if(gift_state){int opened=silph_gift_open(gift_state,silph_gift_size(),read_state,managed_state,game_state,&identity,session_config.flags);if(opened)gift_metrics.error=(uint32_t)opened;}}
        if(gift_state){uint64_t begin=now_ns();int gifts=silph_gift_tick(gift_state,permit,&snapshot,begin,&gift_metrics);uint64_t spent=now_ns()-begin;
            atomic_fetch_add(&progress[PILOT_AUTO_GIFT_NS],spent);if(spent>atomic_load(&progress[PILOT_AUTO_GIFT_MAX_NS]))atomic_store(&progress[PILOT_AUTO_GIFT_MAX_NS],spent);
            if(gifts==SILPH_MANAGED_STALE){atomic_store(&progress[PILOT_ERROR],2000+gifts);return 1;}}
        atomic_store(&progress[PILOT_AUTO_GIFT_STATE],gift_metrics.state);atomic_store(&progress[PILOT_AUTO_GIFT_ERROR],gift_metrics.error);
        atomic_store(&progress[PILOT_AUTO_GIFT_OPERATION],gift_metrics.operation);atomic_store(&progress[PILOT_AUTO_GIFT_RESULT],gift_metrics.result);
        atomic_store(&progress[PILOT_AUTO_GIFT_OPENED],gift_metrics.opened);atomic_store(&progress[PILOT_AUTO_GIFT_SENT],gift_metrics.sent);
        atomic_store(&progress[PILOT_AUTO_GIFT_REQUESTS],gift_metrics.requests);atomic_store(&progress[PILOT_AUTO_GIFT_SKIPPED],gift_metrics.skipped);
        atomic_store(&progress[PILOT_AUTO_GIFT_CONVERTED],gift_metrics.converted);atomic_store(&progress[PILOT_AUTO_GIFT_DUST],gift_metrics.stardust);
    }
#endif
    atomic_store(&progress[PILOT_SAMPLES],snapshot.samples);atomic_store(&progress[PILOT_OWNED],snapshot.pokemon_count);
    atomic_store(&progress[PILOT_MAX_OWNED],snapshot.max_pokemon);atomic_store(&progress[PILOT_ITEMS],snapshot.item_count);
    atomic_store(&progress[PILOT_MAX_ITEMS],snapshot.max_items);atomic_store(&progress[PILOT_WILD],snapshot.map_pokemon);
    atomic_store(&progress[PILOT_STOPS],snapshot.map_stops);atomic_store(&progress[PILOT_CANDIDATES],snapshot.candidates_read);
    atomic_store(&progress[PILOT_FIRST_ID],snapshot.first_encounter);atomic_store(&progress[PILOT_SPECIES],snapshot.first_species);
    atomic_store(&progress[PILOT_ACCOUNT_HI],snapshot.account_hi);atomic_store(&progress[PILOT_ACCOUNT_LO],snapshot.account_lo);
    for(unsigned i=0;i<8;i++)atomic_store(&progress[PILOT_ITEM0+i],snapshot.items[i]);
#ifdef SILPH_PILOT_RPC_READ
    if(atomic_load(&progress[PILOT_RPC_PREPARED])!=SILPH_RPC_COUNT){
        status=silph_rpc_enable(read_state,resolve_managed,&game_image);
        if(!status)status=silph_proto_enable(read_state,resolve_managed,&game_image);
        SilphManagedRef handler={0};
        if(!status)status=silph_game_read_rpc_handler(game_state,&handler);
        for(unsigned i=1;!status&&i<=SILPH_RPC_COUNT;i++){
            status=silph_rpc_prepare(read_state,i,handler);
            if(!status)atomic_store(&progress[PILOT_RPC_PREPARED],i);
            SilphRequestInput input={.abi=1,.encounter=UINT64_MAX,.latitude=40.7,.longitude=-73.9,.player_latitude=40.7,.player_longitude=-73.9,
                .item=i==SILPH_RPC_KECLEON_BERRY?708:1,.count=1,.quality=4,.curve=1,.fort="silph-native-contract",.location="89c25a1"};
            SilphManagedRef proto={0};
            if(!status)status=silph_proto_request(read_state,i,&input,&proto);
            SilphReadValue field={0};
            const char *name=i==SILPH_RPC_KECLEON_GET?"FortId":i==SILPH_RPC_SPIN?"Id":i==SILPH_RPC_DISCARD?"Count":"EncounterId";
            uint32_t kind=i==SILPH_RPC_KECLEON_GET||i==SILPH_RPC_SPIN?SILPH_READ_REFERENCE:i==SILPH_RPC_DISCARD?SILPH_READ_I32:SILPH_READ_U64;
            if(!status)status=silph_read_field(read_state,proto,name,kind,&field);
            if(!status&&kind==SILPH_READ_REFERENCE){char text[128];uint32_t length=0;status=silph_read_utf8(read_state,field.reference,text,sizeof(text),&length);if(!status&&strcmp(text,input.fort))status=SILPH_MANAGED_CONTRACT;}
            if(!status&&kind==SILPH_READ_U64&&field.bits!=UINT64_MAX)status=SILPH_MANAGED_CONTRACT;
            if(!status&&kind==SILPH_READ_I32&&field.bits!=1)status=SILPH_MANAGED_CONTRACT;
            int a=proto.serial?silph_managed_release(managed_state,proto):0,b=field.reference.serial?silph_managed_release(managed_state,field.reference):0;
            if(!status)status=a?a:b;
            if(!status)atomic_store(&progress[PILOT_PROTO_VALIDATED],i);
        }
        SilphReadValue value={0};
        if(!status&&silph_action_rpc(read_state,permit,SILPH_RPC_KECLEON_CAPTURE,handler,(SilphManagedRef){0},&value)!=SILPH_MANAGED_STATE)status=SILPH_MANAGED_CONTRACT;
        int freed=handler.serial?silph_managed_release(managed_state,handler):0;if(!status)status=freed;
        if(status){atomic_store(&progress[PILOT_ERROR],1200+status);return 1;}
    }
#endif
#ifdef SILPH_PILOT_ACTIONS
#ifdef SILPH_PILOT_CONFIGURED
    if(!(session_config.flags&SILPH_CFG_READONLY)
#ifdef SILPH_PILOT_SERVICE
        &&!auxiliary_wait
#endif
        ){
#endif
    SilphFarmerMetrics actions={0};
#ifdef SILPH_PILOT_KECLEON
    SilphKecleonMetrics kec={0};struct timespec wall={0};if(clock_gettime(CLOCK_REALTIME,&wall))status=SILPH_MANAGED_STATE;
#ifdef SILPH_PILOT_SERVICE
    if(!kec_session_started){SilphScheduleIdentity real=config.identity;real.account_hi=snapshot.account_hi;real.account_lo=snapshot.account_lo;
        if(!status)status=silph_kecleon_session_open(farmer_state,silph_kecleon_session_size(),read_state,managed_state,game_state,session_link,&real,&session_config,now_ns());
        if(!status)kec_session_started=1;
    }
    if(!status)status=silph_kecleon_session_tick(farmer_state,permit,&snapshot,now_ns(),(uint64_t)wall.tv_sec*1000+(uint64_t)wall.tv_nsec/1000000,&kec_session);
    kec=kec_session.current;
    actions.encounters=kec_session.plan.encounters;actions.throws=kec_session.plan.throws;actions.catches=kec_session.plan.catches;
    if(kec_session.plan.phase==SILPH_KPLAN_RUN){actions.encounters+=kec.encounters;actions.throws+=kec.throws;actions.catches+=kec.catches;}
    actions.captured_id=kec.captured_id;actions.target_id=kec.encounter;actions.state=kec_session.plan.phase==SILPH_KPLAN_FINISHED?SILPH_FARMER_DONE:SILPH_FARMER_SELECT;
#else
    if(!status)status=silph_kecleon_actions_tick(farmer_state,permit,&snapshot,now_ns(),(uint64_t)wall.tv_sec*1000+(uint64_t)wall.tv_nsec/1000000,&kec);
    actions.encounters=kec.encounters;actions.throws=kec.throws;actions.catches=kec.catches;actions.captured_id=kec.captured_id;actions.target_id=kec.encounter;
    actions.state=kec.state==SILPH_KECLEON_DONE||kec.state==SILPH_KECLEON_UNVERIFIED?SILPH_FARMER_DONE:SILPH_FARMER_SELECT;
#endif
    actions.status=kec.status;actions.species=352;actions.error=kec.error;actions.fresh_response=kec.fresh_capture;
    atomic_store(&progress[PILOT_KECLEON_STATE],kec.state);atomic_store(&progress[PILOT_KECLEON_VERIFIED],kec.verified);
    atomic_store(&progress[PILOT_KECLEON_SHINY],kec.shiny);atomic_store(&progress[PILOT_KECLEON_CP],kec.cp);
    atomic_store(&progress[PILOT_KECLEON_ATTACK],kec.attack);atomic_store(&progress[PILOT_KECLEON_DEFENSE],kec.defense);atomic_store(&progress[PILOT_KECLEON_STAMINA],kec.stamina);
    atomic_store(&progress[PILOT_KECLEON_BERRIES],kec.berries);atomic_store(&progress[PILOT_KECLEON_REQUESTS],kec.requests);
#elif defined(SILPH_PILOT_SPIN)
    SilphMaintenanceMetrics maintenance={0};SilphScheduleIdentity real=config.identity;real.account_hi=snapshot.account_hi;real.account_lo=snapshot.account_lo;
    if(!atomic_load(&progress[PILOT_SPIN_STARTED])){
        status=silph_maintenance_open(farmer_state,silph_maintenance_size(),read_state,managed_state,game_state,&real);
        if(!status){
#ifdef SILPH_PILOT_DISCARD
            status=silph_maintenance_discard(farmer_state,permit,&snapshot,703,10,1,now_ns());
#else
            status=silph_maintenance_spin(farmer_state,permit,&snapshot,now_ns());
#endif
            if(!status)atomic_store(&progress[PILOT_SPIN_STARTED],1);
        }
    }
    if(!status)status=silph_maintenance_poll(farmer_state,&real,now_ns(),&maintenance);
    actions.state=maintenance.state==3||maintenance.state==4?SILPH_FARMER_DONE:SILPH_FARMER_SELECT;
    actions.error=maintenance.error;
    atomic_store(&progress[PILOT_SPIN_COUNT],maintenance.spins);atomic_store(&progress[PILOT_SPIN_RESULT],maintenance.result);
    atomic_store(&progress[PILOT_SPIN_REQUESTS],maintenance.requests);atomic_store(&progress[PILOT_SPIN_XP],maintenance.xp);
    atomic_store(&progress[PILOT_DISCARD_COUNT],maintenance.discarded);atomic_store(&progress[PILOT_DISCARD_ITEM],maintenance.item);
    atomic_store(&progress[PILOT_DISCARD_BEFORE],maintenance.before);atomic_store(&progress[PILOT_DISCARD_AFTER],maintenance.after);
    atomic_store(&progress[PILOT_DISCARD_REQUESTED],maintenance.requested);atomic_store(&progress[PILOT_DISCARD_FLOOR],maintenance.floor);
#elif defined(SILPH_PILOT_COMBINED)
    SilphScheduleIdentity real=config.identity;real.account_hi=snapshot.account_hi;real.account_lo=snapshot.account_lo;
    if(!session_started){
        SilphFarmerSessionOptions options={.abi=1,.catches=1,.spins=1,.max_spins=5,
            .farmer={.abi=1,.continuous=1,.max_catches=3,.quality=4,.curve=1,.use_berries=1,.keep_options=127,.berry_count=3,.berries={701,705,703}}};
        unsigned transfers=1;
#ifdef SILPH_PILOT_CONFIGURED
        options=(SilphFarmerSessionOptions){.abi=1,.catches=!!(session_config.flags&SILPH_CFG_CATCH),.spins=!!(session_config.flags&SILPH_CFG_SPIN),
            .clean=!!(session_config.flags&SILPH_CFG_CLEAN),.max_spins=session_config.max_spins,.limit_count=session_config.limit_count,
            .farmer={.abi=1,.continuous=1,.max_catches=session_config.max_catches,.quality=session_config.quality,.curve=!!(session_config.flags&SILPH_CFG_CURVE),
                .use_berries=!!(session_config.flags&SILPH_CFG_BERRIES),.keep_options=session_config.keep,.berry_count=session_config.berry_count}};
        for(unsigned i=0;i<5;i++)options.farmer.berries[i]=(int32_t)session_config.berries[i];
        for(unsigned i=0;i<64;i++){options.limits[i].item=(int32_t)session_config.limits[i].item;options.limits[i].floor=session_config.limits[i].floor;}
        memcpy(options.farmer.only_species,session_config.species,sizeof(options.farmer.only_species));transfers=!!(session_config.flags&SILPH_CFG_TRANSFER);
#endif
#ifdef SILPH_PILOT_SERVICE
        options.research=1;
#endif
        status=silph_farmer_session_open(farmer_state,silph_farmer_session_size(),read_state,managed_state,game_state,&real,&options,transfers);
        if(!status)session_started=1;
    }
    SilphFarmerSessionMetrics session={0};
#ifdef SILPH_PILOT_SERVICE
    bool watch=(session_config.flags&SILPH_CFG_CATCH)!=0;for(unsigned i=0;i<256;i++)if(session_config.species[i])watch=false;
    uint64_t now=now_ns();
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    if(silph_farmer_view_ready(farmer_view)&&farmer_view_kind!=SILPH_VIEW_MAP){silph_recovery_pause(&recovery);watch=false;}
#endif
    if(watch&&now>=recovery_due){uint32_t count=0;int read=silph_game_read_map_count(game_state,&count);recovery_due=now+UINT64_C(1000000000);
        bool expired=silph_recovery_observe(&recovery,now,count,!read);if(expired&&silph_farmer_session_idle(farmer_state)){auxiliary_reason=10;silph_schedule_stop(schedule,lease);}}
    if(!watch)silph_recovery_pause(&recovery);
    if(auxiliary_reason==10)session=previous_session;
    else if(!status)status=silph_farmer_session_tick(farmer_state,permit,&snapshot,now,&session);
    previous_session=session;
#else
    if(!status)status=silph_farmer_session_tick(farmer_state,permit,&snapshot,now_ns(),&session);
#endif
    actions=session.farmer;if(session.state!=SILPH_SESSION_RUNNING)actions.state=SILPH_FARMER_DONE;
#ifdef SILPH_PILOT_SERVICE
    session_research=session.research;session_xp=actions.xp+session.spin_xp+session.research.xp+session.go_pass.xp;session_dust=actions.stardust+session.spin_stardust+session.research.stardust+session.go_pass.stardust;
    session_reward_error=actions.reward_error?actions.reward_error:session.reward_error?session.reward_error:session.research.reward_error;session_fled=actions.fled;
    atomic_store(&progress[PILOT_RESEARCH_SCANS],session.research.scans);atomic_store(&progress[PILOT_RESEARCH_CLAIMS],session.research.claimed);
    atomic_store(&progress[PILOT_RESEARCH_REMOVED],session.research.discarded);atomic_store(&progress[PILOT_RESEARCH_ENCOUNTERS],session.research.encounters);
    atomic_store(&progress[PILOT_RESEARCH_STATE],session.research.state);atomic_store(&progress[PILOT_RESEARCH_ERROR],session.research.error);
    atomic_store(&progress[PILOT_RESEARCH_OPERATION],session.research.operation);atomic_store(&progress[PILOT_RESEARCH_RESULT],session.research.result);
    atomic_store(&progress[PILOT_RESEARCH_NS],session.research_ns);atomic_store(&progress[PILOT_RESEARCH_MAX_NS],session.max_research_ns);
    atomic_store(&progress[PILOT_RESEARCH_CATCHES],actions.research_catches);atomic_store(&progress[PILOT_RESEARCH_TRANSFERRED],actions.research_transferred);atomic_store(&progress[PILOT_RESEARCH_REQUESTS],session.research.requests);
    atomic_store(&progress[PILOT_WEEKLY_CLAIMS],session.research.weekly_claimed);atomic_store(&progress[PILOT_WEEKLY_ERROR],session.research.weekly_error);
    atomic_store(&progress[PILOT_PASS_STATE],session.go_pass.state);atomic_store(&progress[PILOT_PASS_ERROR],session.go_pass.error);atomic_store(&progress[PILOT_PASS_OPERATION],session.go_pass.operation);atomic_store(&progress[PILOT_PASS_RESULT],session.go_pass.result);
    atomic_store(&progress[PILOT_PASS_SCANS],session.go_pass.scans);atomic_store(&progress[PILOT_PASS_REQUESTS],session.go_pass.requests);atomic_store(&progress[PILOT_PASS_CLAIMS],session.go_pass.claimed);atomic_store(&progress[PILOT_PASS_FOUND],session.go_pass.encounters);
    atomic_store(&progress[PILOT_PASS_CATCHES],actions.go_pass_catches);atomic_store(&progress[PILOT_PASS_TRANSFERRED],actions.go_pass_transferred);atomic_store(&progress[PILOT_PASS_XP],session.go_pass.xp);atomic_store(&progress[PILOT_PASS_DUST],session.go_pass.stardust);atomic_store(&progress[PILOT_PASS_NS],session.go_pass.work_ns);atomic_store(&progress[PILOT_PASS_MAX_NS],session.go_pass.max_work_ns);
#endif
    atomic_store(&progress[PILOT_SESSION_STATE],session.state);atomic_store(&progress[PILOT_SESSION_REASON],session.reason);
    atomic_store(&progress[PILOT_SESSION_SPINS],session.spins);atomic_store(&progress[PILOT_SESSION_SPIN_REQUESTS],session.spin_requests);
    atomic_store(&progress[PILOT_SESSION_DELETED],session.deleted);atomic_store(&progress[PILOT_SESSION_DELETE_REQUESTS],session.delete_requests);
    atomic_store(&progress[PILOT_SESSION_SPIN_XP],session.spin_xp);atomic_store(&progress[PILOT_SESSION_FARMER_NS],session.farmer_ns);
    atomic_store(&progress[PILOT_SESSION_MAINTENANCE_NS],session.maintenance_ns);atomic_store(&progress[PILOT_SESSION_MAX_FARMER_NS],session.max_farmer_ns);
    atomic_store(&progress[PILOT_SESSION_MAX_MAINTENANCE_NS],session.max_maintenance_ns);atomic_store(&progress[PILOT_SESSION_BERRIES],actions.berries);
    atomic_store(&progress[PILOT_SESSION_BERRY_REQUESTS],actions.berry_requests);
    atomic_store(&progress[PILOT_SESSION_FOLLOWUPS],session.followups);
    atomic_store(&progress[PILOT_SESSION_REFRESHES],session.refreshes);
#else
    status=silph_farmer_actions_tick(farmer_state,permit,&snapshot,now_ns(),&actions);
#endif
    atomic_store(&progress[PILOT_ACTION_ENCOUNTERS],actions.encounters);atomic_store(&progress[PILOT_ACTION_THROWS],actions.throws);
    atomic_store(&progress[PILOT_ACTION_CATCHES],actions.catches);atomic_store(&progress[PILOT_ACTION_CAPTURED_ID],actions.captured_id);
    atomic_store(&progress[PILOT_ACTION_TARGET_ID],actions.target_id);atomic_store(&progress[PILOT_ACTION_FRESH],actions.fresh_response);
    atomic_store(&progress[PILOT_ACTION_STATE],actions.state);atomic_store(&progress[PILOT_ACTION_STATUS],actions.status);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    bool continuation=session_config.delay_ms==100&&silph_farmer_view_ready(farmer_view)
        &&actions.state!=SILPH_FARMER_SELECT&&actions.state!=SILPH_FARMER_DONE&&actions.state!=0;
    atomic_store(&farmer_continuation,continuation);silph_farmer_view_continuation(farmer_view,continuation);
    atomic_store(&progress[PILOT_VIEW_RESERVED],continuation?33:session_config.delay_ms);
#endif
    atomic_store(&progress[PILOT_ACTION_SPECIES],actions.species);atomic_store(&progress[PILOT_ACTION_ERROR],status?status:actions.error);
    atomic_store(&progress[PILOT_ACTION_TRANSFER_REQUESTS],actions.transfer_requests);atomic_store(&progress[PILOT_ACTION_TRANSFERRED],actions.transferred);
    atomic_store(&progress[PILOT_ACTION_TRANSFER_DECISION],actions.transfer_decision);atomic_store(&progress[PILOT_ACTION_TRANSFER_REASONS],actions.transfer_reasons);
    atomic_store(&progress[PILOT_ACTION_TRANSFER_ERROR],actions.transfer_error);atomic_store(&progress[PILOT_ACTION_SCOPE],actions.scope_granted);
#ifdef SILPH_PILOT_SERVICE
    atomic_store(&progress[PILOT_ACTION_SKIPPED],actions.skipped);atomic_store(&progress[PILOT_PROMISE_PHASE],actions.promise_phase);atomic_store(&progress[PILOT_PROMISE_STATE],actions.promise_state);atomic_store(&progress[PILOT_PROMISE_REASON],actions.promise_reason);
#endif
    atomic_store(&progress[PILOT_SAMPLES],silph_game_read_samples(game_state));
    if(status){atomic_store(&progress[PILOT_ERROR],1100+status);return 1;}
    if(actions.state==SILPH_FARMER_DONE)silph_schedule_stop(schedule,lease);
#ifdef SILPH_PILOT_CONFIGURED
    }
    session_snapshot=(SilphSessionSnapshot){.pid=config.identity.pid,.ticks=config.identity.process_ticks,.session_hi=config.identity.session_hi,.session_lo=config.identity.session_lo,
        .sequence=++publication_sequence,.mode=session_config.mode,.state=2,.flags=(session_config.flags&SILPH_CFG_READONLY?1:0)|2,
        .account_hi=snapshot.account_hi,.account_lo=snapshot.account_lo,
        .caught=atomic_load(&progress[PILOT_ACTION_CATCHES]),.transferred=atomic_load(&progress[PILOT_ACTION_TRANSFERRED]),
        .encounters=atomic_load(&progress[PILOT_ACTION_ENCOUNTERS]),.throws=atomic_load(&progress[PILOT_ACTION_THROWS]),
        .encounter=atomic_load(&progress[PILOT_ACTION_TARGET_ID]),.captured=atomic_load(&progress[PILOT_ACTION_CAPTURED_ID])};
#ifdef SILPH_PILOT_COMBINED
    session_snapshot.spun=atomic_load(&progress[PILOT_SESSION_SPINS]);session_snapshot.discarded=atomic_load(&progress[PILOT_SESSION_DELETED]);
    session_snapshot.berries=atomic_load(&progress[PILOT_SESSION_BERRIES]);
    session_snapshot.reason=(uint32_t)atomic_load(&progress[PILOT_SESSION_REASON]);
#endif
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_COMBINED)
    if(!(session_config.flags&SILPH_CFG_READONLY)){session_snapshot.xp=session_xp;session_snapshot.stardust=session_dust;session_snapshot.rewards_available=session_reward_error?0:3;session_snapshot.fled=session_fled;}
    session_snapshot.candidate_request=session_research.claimed|(session_research.discarded<<32);session_snapshot.candidate_state=session_research.state;
    session_snapshot.research_status=session_research.error?session_research.operation*100+session_research.error:0;
#endif
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_KECLEON)
    session_snapshot.discarded=kec_session.deleted;session_snapshot.candidate_request=kec_session.plan.request;session_snapshot.candidate_state=kec_session.plan.phase;
    session_snapshot.reason=kec_session.plan.reason;session_snapshot.shinies=kec_session.plan.shinies;session_snapshot.berries=kec_session.plan.berries;
    if(kec_session.plan.phase==SILPH_KPLAN_RUN){session_snapshot.shinies+=kec_session.current.shinies;session_snapshot.berries+=kec_session.current.berries;}
    if(!(session_config.flags&SILPH_CFG_READONLY)){
        session_snapshot.xp=kec_session.plan.xp;session_snapshot.stardust=kec_session.plan.stardust;session_snapshot.rewards_available=kec_session.plan.reward_error?0:3;
        if(kec_session.plan.phase==SILPH_KPLAN_RUN){session_snapshot.xp+=kec_session.current.xp;session_snapshot.stardust+=kec_session.current.stardust;if(kec_session.current.reward_error)session_snapshot.rewards_available=0;}
    }
#endif
#ifdef SILPH_PILOT_SERVICE
    if(!(session_config.flags&SILPH_CFG_READONLY)){session_snapshot.xp+=gift_metrics.xp;session_snapshot.stardust+=gift_metrics.stardust;}
    session_snapshot.visual_available=visual_metrics.available;session_snapshot.visual_failed=visual_metrics.failed;session_snapshot.iv_reads=visual_metrics.iv_reads;session_snapshot.name_updates=visual_metrics.name_updates;
    session_snapshot.visual_available|=presentation_metrics.available;session_snapshot.visual_failed|=presentation_metrics.failed;
    if(auxiliary_reason)session_snapshot.reason=auxiliary_reason;
    session_snapshot.location_status=(session_config.flags&SILPH_CFG_FIX_LOCATION)?(atomic_load(&progress[PILOT_LOCATION_STATE])==1?1:atomic_load(&progress[PILOT_LOCATION_STATE])==2?2:100+atomic_load(&progress[PILOT_LOCATION_ERROR])):0;
#endif
    int published=silph_session_link_publish(session_link,&session_snapshot);if(published&&published!=SILPH_SCHEDULE_BUSY){atomic_store(&progress[PILOT_ERROR],1500+published);return 1;}
#if defined(SILPH_PILOT_SERVICE) && defined(SILPH_PILOT_KECLEON)
    if(!published&&kec_session_started){int event_status=silph_kecleon_session_publish(farmer_state);if(event_status){atomic_store(&progress[PILOT_ERROR],1600+event_status);return 1;}}
#endif
#endif
#endif
#else
    int32_t length=0;
    if(!status)status=silph_managed_invoke_i32(managed_state,getter,text,&length);
    if(status||length!=15){atomic_store(&progress[PILOT_ERROR],100+status);return 1;}
#endif
    atomic_fetch_add(&progress[PILOT_JOBS],1);return 0;
}
static int cleanup(void *unused) {
    (void)unused;atomic_store_explicit(&enabled,0,memory_order_release);atomic_fetch_add(&progress[PILOT_CLEANUP],1);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    atomic_store(&farmer_continuation,0);
#endif
    int status=0;SilphManagedMetrics metrics={0};
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    if(farmer_view){status=silph_farmer_view_close(farmer_view,&farmer_view_metrics);farmer_view_progress();if(!status){free(farmer_view);farmer_view=NULL;}}
#endif
#ifdef SILPH_PILOT_SERVICE
    int location_close_status=location_attempted&&(session_config.flags&SILPH_CFG_FIX_LOCATION)?silph_location_close():0;location_progress();
#endif
#ifdef SILPH_PILOT_CONFIGURED
    if(!session_configured&&!silph_session_link_config(session_link,&session_config))session_configured=1;
#endif
#ifdef SILPH_PILOT_ACTIONS
#ifdef SILPH_PILOT_KECLEON
#ifdef SILPH_PILOT_SERVICE
    if(farmer_started&&kec_session_started)status=silph_kecleon_session_close(farmer_state);
#else
    if(farmer_started)status=silph_kecleon_actions_close(farmer_state);
#endif
#elif defined(SILPH_PILOT_SPIN)
    if(farmer_started)status=silph_maintenance_close(farmer_state);
#elif defined(SILPH_PILOT_COMBINED)
    if(farmer_started)status=silph_farmer_session_close(farmer_state);
#else
    if(farmer_started)status=silph_farmer_actions_close(farmer_state);
#endif
    if(!status){free(farmer_state);farmer_state=NULL;farmer_started=0;}
#endif
#ifdef SILPH_PILOT_GAME_READ
#ifdef SILPH_PILOT_SERVICE
    if(gift_state){int closed=silph_gift_close(gift_state);if(!status)status=closed;if(!closed){free(gift_state);gift_state=NULL;}}
    if(buddy_state){int closed=silph_buddy_close(buddy_state);if(!status)status=closed;if(!closed){free(buddy_state);buddy_state=NULL;}}
    if(presentation_state){int closed=silph_presentation_close(presentation_state,&presentation_metrics);if(!status)status=closed;if(!closed){free(presentation_state);presentation_state=NULL;}}
    presentation_progress();
#endif
    if(!status&&game_started)status=silph_game_read_close(game_state);
    if(!status&&read_started)status=silph_read_close(read_state);
    if(!status){free(game_state);free(read_state);game_state=read_state=NULL;game_started=read_started=0;}
#endif
#ifdef SILPH_PILOT_SERVICE
    if(visual_state){int closed=silph_visual_close(visual_state,&visual_metrics);if(!status)status=closed;if(!closed){free(visual_state);visual_state=NULL;}}
#endif
    if(managed_state) {
        int closed=silph_managed_close(managed_state,&metrics);if(!status)status=closed;
        atomic_store(&progress[PILOT_CREATED],metrics.refs_created);atomic_store(&progress[PILOT_RELEASED],metrics.refs_released);
        atomic_store(&progress[PILOT_REMAINING],metrics.live_refs);
        if(!status){free(managed_state);managed_state=NULL;}
    }
#ifdef SILPH_PILOT_SERVICE
    atomic_fetch_add(&progress[PILOT_CREATED],visual_metrics.created);atomic_fetch_add(&progress[PILOT_RELEASED],visual_metrics.released);atomic_fetch_add(&progress[PILOT_REMAINING],visual_metrics.remaining);
    atomic_fetch_add(&progress[PILOT_CREATED],presentation_metrics.created);atomic_fetch_add(&progress[PILOT_RELEASED],presentation_metrics.released);atomic_fetch_add(&progress[PILOT_REMAINING],presentation_metrics.remaining);
#ifdef SILPH_PILOT_COMBINED
    atomic_fetch_add(&progress[PILOT_CREATED],farmer_view_metrics.created);atomic_fetch_add(&progress[PILOT_RELEASED],farmer_view_metrics.released);atomic_fetch_add(&progress[PILOT_REMAINING],farmer_view_metrics.remaining);
#endif
#endif
#ifdef SILPH_PILOT_SERVICE
    if(!status)status=location_close_status;
#endif
    if(status)atomic_store(&progress[PILOT_ERROR],200+status);
#ifdef SILPH_PILOT_CONFIGURED
    if(session_configured){
        session_snapshot.pid=config.identity.pid;session_snapshot.ticks=config.identity.process_ticks;session_snapshot.session_hi=config.identity.session_hi;
        session_snapshot.session_lo=config.identity.session_lo;session_snapshot.mode=session_config.mode;session_snapshot.sequence=++publication_sequence;
        session_snapshot.error=status?(uint32_t)status:(uint32_t)atomic_load(&progress[PILOT_ERROR]);session_snapshot.state=session_snapshot.error?6:5;
        session_snapshot.flags=(session_config.flags&SILPH_CFG_READONLY?1:0)|8|(status?0:4);
#ifdef SILPH_PILOT_SERVICE
        session_snapshot.visual_available=visual_metrics.available;session_snapshot.visual_failed=visual_metrics.failed;session_snapshot.iv_reads=visual_metrics.iv_reads;session_snapshot.name_updates=visual_metrics.name_updates;
        session_snapshot.visual_available|=presentation_metrics.available;session_snapshot.visual_failed|=presentation_metrics.failed;
        session_snapshot.location_status=(session_config.flags&SILPH_CFG_FIX_LOCATION)?(atomic_load(&progress[PILOT_LOCATION_STATE])==2?2:100+atomic_load(&progress[PILOT_LOCATION_ERROR])):0;
#endif
        int published=SILPH_SCHEDULE_BUSY;for(unsigned i=0;i<3&&published==SILPH_SCHEDULE_BUSY;i++)published=silph_session_link_publish(session_link,&session_snapshot);
        if(published&&!status)status=published;
    }
#endif
    atomic_store(&progress[PILOT_STOPPED],!status);return status;
}
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
static void farmer_pump(void *object,bool allowed,uint32_t kind) {
    if(!atomic_load(&enabled)||(unsigned)syscall(__NR_gettid)!=atomic_load(&owner))return;
    prepare_tools();
    if(session_configured&&(session_config.flags&SILPH_CFG_READONLY)){
        if(farmer_view&&!silph_farmer_view_close(farmer_view,&farmer_view_metrics)){free(farmer_view);farmer_view=NULL;}
        return;
    }
    if(presentation_state)silph_presentation_map(presentation_state,object,now_ns());
    farmer_view_kind=kind;farmer_view_progress();
    if(presentation_state&&silph_presentation_busy(presentation_state))return;
    void *previous=active_map;active_map=object;
    int tick=silph_schedule_tick(schedule,lease,&config.identity,allowed);
    if(visual_state)silph_visual_prime(visual_state,object);
    if(presentation_state&&game_started)silph_presentation_prime(presentation_state,game_state,managed_state);
    if(!allowed)silph_recovery_pause(&recovery);
    if(presentation_state)silph_presentation_drain(presentation_state,now_ns());active_map=previous;
    if(tick==SILPH_SCHEDULE_BUSY)atomic_fetch_add(&progress[PILOT_BUSY],1);
    else if(tick&&tick!=SILPH_SCHEDULE_STATE){atomic_store(&progress[PILOT_ERROR],400+tick);atomic_store(&enabled,0);}
    SilphScheduleMetrics metrics;if(!silph_schedule_metrics(schedule,lease,&metrics)){
        atomic_store(&progress[PILOT_SCHED_STATE],metrics.state);atomic_store(&progress[PILOT_QUEUE],metrics.queued);atomic_store(&progress[PILOT_REASON],metrics.reason);
    }
}
#endif
static bool proxy(void *object,const void *method) {
#ifdef SILPH_PILOT_SERVICE
    if(atomic_load(&enabled)&&atomic_load(&owner)==(unsigned)syscall(__NR_gettid))prepare_tools();
#endif
    bool result=original(object,method);
    if(!atomic_load_explicit(&enabled,memory_order_acquire))return result;
    atomic_fetch_add(&progress[PILOT_CALLBACKS],1);
    uint32_t current=(uint32_t)syscall(__NR_gettid),bound=atomic_load_explicit(&owner,memory_order_acquire);
    if(!bound) {
        char name[16]={0};
        if(prctl(PR_GET_NAME,name)||strcmp(name,"UnityMain")||!object||managed_api.object_get_class(object)!=map.klass
            ||!managed_api.thread_current())return result;
        unsigned expected=0;if(!atomic_compare_exchange_strong(&owner,&expected,current))return result;
        config.owner_tid=current;
        int opened=silph_schedule_open(schedule,&config,&lease);
        if(opened){atomic_store(&progress[PILOT_ERROR],300+opened);atomic_store(&enabled,0);return result;}
        atomic_store(&progress[PILOT_OWNER],current);atomic_store_explicit(&progress[PILOT_STATE],PILOT_READY,memory_order_release);bound=current;
    }
    if(current!=bound){atomic_fetch_add(&progress[PILOT_FOREIGN],1);return result;}
#ifdef SILPH_PILOT_SERVICE
    prepare_tools();
#ifdef SILPH_PILOT_COMBINED
    prepare_view(object);
#endif
    if(visual_state)silph_visual_prime(visual_state,object);
    if(presentation_state)silph_presentation_map(presentation_state,object,now_ns());
    if(presentation_state&&silph_presentation_busy(presentation_state))return result;
#endif
    atomic_store(&progress[PILOT_MAP_VISIBLE],result);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
    if(silph_farmer_view_ready(farmer_view))return result;
#endif
#ifdef SILPH_PILOT_GAME_READ
    void *previous_map=active_map;active_map=object;
#endif
    int tick=silph_schedule_tick(schedule,lease,&config.identity,result);
#ifdef SILPH_PILOT_SERVICE
    if(visual_state)silph_visual_prime(visual_state,object);
    if(presentation_state&&game_started)silph_presentation_prime(presentation_state,game_state,managed_state);
    if(!result||!visual_allowed(NULL))silph_recovery_pause(&recovery);
    if(presentation_state)silph_presentation_drain(presentation_state,now_ns());
#endif
#ifdef SILPH_PILOT_GAME_READ
    active_map=previous_map;
#endif
    if(tick==SILPH_SCHEDULE_BUSY)atomic_fetch_add(&progress[PILOT_BUSY],1);
    else if(tick&&tick!=SILPH_SCHEDULE_STATE){atomic_store(&progress[PILOT_ERROR],400+tick);atomic_store(&enabled,0);}
    SilphScheduleMetrics metrics;
    if(!silph_schedule_metrics(schedule,lease,&metrics)) {
        atomic_store(&progress[PILOT_SCHED_STATE],metrics.state);atomic_store(&progress[PILOT_QUEUE],metrics.queued);
        atomic_store(&progress[PILOT_REASON],metrics.reason);
    }
    return result;
}
static void *controller(void *unused) {
    (void)unused;
    uint64_t deadline=now_ns()+UINT64_C(60000000000),sequence=1;
#ifndef SILPH_PILOT_EXTERNAL
    uint64_t last_pulse=0;
#endif
    while(now_ns()<deadline&&atomic_load_explicit(&progress[PILOT_STATE],memory_order_acquire)!=PILOT_READY
        &&!atomic_load(&progress[PILOT_ERROR])){struct timespec pause={0,10000000};nanosleep(&pause,NULL);}
    bool ready=atomic_load_explicit(&progress[PILOT_STATE],memory_order_acquire)==PILOT_READY;
#ifdef SILPH_PILOT_EXTERNAL
    uint64_t last_submit=0;
#ifdef SILPH_PILOT_CONFIGURED
    unsigned configured_deadline=0;uint64_t closed_at=0;
#endif
    if(ready) {
        control_state=calloc(1,silph_control_size());
        SilphControlConfig channel={.scheduler=schedule,.lease=lease,.identity=config.identity,
            .key_hi=config.identity.session_hi,.key_lo=control_key_lo,.controller_uid=0};
#ifdef SILPH_PILOT_CONFIGURED
        channel.message=silph_session_link_message;channel.snapshot=silph_session_link_snapshot;channel.extension=session_link;
#ifdef SILPH_PILOT_READONLY_ONLY
        channel.message=silph_session_link_readonly_message;
#endif
#endif
        int opened=control_state?silph_control_open(control_state,silph_control_size(),&channel):SILPH_CONTROL_ARGUMENT;
        if(opened)atomic_store(&progress[PILOT_ERROR],800+opened);
    }
    while(ready&&now_ns()<deadline&&!atomic_load(&progress[PILOT_ERROR])) {
        int polled=silph_control_poll(control_state,
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
            atomic_load(&farmer_continuation)?15:
#endif
            50);
        if(polled){atomic_store(&progress[PILOT_ERROR],850+polled);break;}
        SilphControlMetrics channel={0};
        if(silph_control_metrics(control_state,&channel)){atomic_store(&progress[PILOT_ERROR],880);break;}
        atomic_store(&progress[PILOT_CONTROL_ACCEPTED],channel.accepted);atomic_store(&progress[PILOT_CONTROL_REJECTED],channel.rejected);
        atomic_store(&progress[PILOT_CONTROL_PULSES],channel.pulses);atomic_store(&progress[PILOT_CONTROL_DISCONNECTS],channel.disconnects);
        atomic_store(&progress[PILOT_CONTROL_SEQUENCE],channel.sequence);atomic_store(&progress[PILOT_PULSES],channel.pulses);
        SilphScheduleMetrics metrics={0};int read=silph_schedule_metrics(schedule,lease,&metrics);
        if(read==SILPH_SCHEDULE_BUSY)continue;
        if(read){atomic_store(&progress[PILOT_ERROR],890+read);break;}
        if(metrics.state==SILPH_SCHEDULE_CLOSED||metrics.state==SILPH_SCHEDULE_UNCERTAIN){
#ifdef SILPH_PILOT_CONFIGURED
            if(!closed_at)closed_at=now_ns();if(now_ns()-closed_at<UINT64_C(1500000000))continue;
#endif
            break;
        }
        uint64_t submit_interval=
#ifdef SILPH_PILOT_CONFIGURED
            UINT64_C(100000000);
        SilphSessionConfig live_config;int configured=silph_session_link_config(session_link,&live_config);
        if(!configured){
#ifdef SILPH_PILOT_SERVICE
            unsigned bootstrap=atomic_load(&location_bootstrap_state);
            if(bootstrap){
                if(bootstrap==1)continue;
                if(bootstrap==2&&(live_config.flags&SILPH_CFG_FIX_LOCATION)){
                    unsigned expected=2;if(!atomic_compare_exchange_strong(&location_bootstrap_state,&expected,4))continue;
                    atomic_store(&location_bootstrap_until,0);
                }else if(bootstrap!=4||!(live_config.flags&SILPH_CFG_FIX_LOCATION)){atomic_store(&progress[PILOT_ERROR],1741);break;}
            }
            if(!atomic_exchange(&location_attempted,1)){
                if(live_config.flags&SILPH_CFG_FIX_LOCATION){void *get_vms=NULL;dl_iterate_phdr(location_symbol,&get_vms);int installed=silph_location_open(get_vms,location_allowed,NULL);location_progress();if(installed){atomic_store(&progress[PILOT_ERROR],1700+installed);break;}}
            }
#endif
            submit_interval=(uint64_t)live_config.delay_ms*UINT64_C(1000000);
#if defined(SILPH_PILOT_COMBINED) && defined(SILPH_PILOT_SERVICE)
            if(live_config.delay_ms==100&&atomic_load(&farmer_continuation))submit_interval=UINT64_C(33000000);
#endif
            if(!configured_deadline){configured_deadline=1;deadline=live_config.duration_ms?now_ns()+(uint64_t)live_config.duration_ms*UINT64_C(1000000):UINT64_MAX;}
        }
#elif defined(SILPH_PILOT_CONTINUOUS)
            UINT64_C(50000000);
#else
            UINT64_C(200000000);
#endif
        if(metrics.state==SILPH_SCHEDULE_RUNNING&&!metrics.queued&&now_ns()-last_submit>=
            submit_interval
        ) {
            SilphScheduleJob job={.sequence=sequence,.deadline_ns=now_ns()+UINT64_C(3000000000),
#if defined(SILPH_PILOT_ACTIONS) && !defined(SILPH_PILOT_READONLY_ONLY)
                .kind=SILPH_JOB_WRITE
#else
                .kind=SILPH_JOB_OBSERVE
#endif
            };
            int submitted=silph_schedule_submit(schedule,lease,&job);
            if(!submitted){sequence++;last_submit=now_ns();}
            else if(submitted!=SILPH_SCHEDULE_BUSY&&submitted!=SILPH_SCHEDULE_STATE){atomic_store(&progress[PILOT_ERROR],900+submitted);break;}
        }
    }
#else
    while(ready&&now_ns()<deadline&&sequence<=SILPH_PILOT_GOAL&&!atomic_load(&progress[PILOT_ERROR])) {
        int pulse=SILPH_SCHEDULE_OK;
        if(now_ns()-last_pulse>=UINT64_C(200000000)) {
        pulse=silph_schedule_pulse(schedule,lease,&config.identity,true);
        if(pulse==SILPH_SCHEDULE_OK)atomic_fetch_add(&progress[PILOT_PULSES],1);
        if(pulse==SILPH_SCHEDULE_OK)last_pulse=now_ns();
        }
        if(pulse!=SILPH_SCHEDULE_OK&&pulse!=SILPH_SCHEDULE_BUSY)break;
        if(atomic_load(&progress[PILOT_JOBS])<sequence-1){struct timespec pause={0,50000000};nanosleep(&pause,NULL);continue;}
        SilphScheduleJob job={.sequence=sequence,.deadline_ns=now_ns()+UINT64_C(3000000000),.kind=SILPH_JOB_OBSERVE};
        int submitted=silph_schedule_submit(schedule,lease,&job);
        if(submitted==SILPH_SCHEDULE_OK)sequence++;
        else if(submitted!=SILPH_SCHEDULE_BUSY&&submitted!=SILPH_SCHEDULE_CAPACITY_ERROR)break;
        struct timespec pause={0,100000000};nanosleep(&pause,NULL);
    }
    while(ready&&now_ns()<deadline&&atomic_load(&progress[PILOT_JOBS])<SILPH_PILOT_GOAL&&!atomic_load(&progress[PILOT_ERROR])) {
        if(now_ns()-last_pulse>=UINT64_C(200000000)){silph_schedule_pulse(schedule,lease,&config.identity,true);last_pulse=now_ns();}
        struct timespec pause={0,50000000};nanosleep(&pause,NULL);
    }
#endif
    uint64_t cleanup_deadline=now_ns()+UINT64_C(5000000000);
    if(ready) {
        silph_schedule_stop(schedule,lease);
        while(now_ns()<cleanup_deadline&&!atomic_load(&progress[PILOT_STOPPED])) {
            struct timespec pause={0,10000000};nanosleep(&pause,NULL);
        }
    }
#ifdef SILPH_PILOT_EXTERNAL
    if(control_state)silph_control_close(control_state);
#endif
    atomic_store_explicit(&enabled,0,memory_order_release);
#ifdef SILPH_PILOT_SERVICE
    if(location_attempted&&(session_config.flags&SILPH_CFG_FIX_LOCATION)){int closed=silph_location_close();if(closed&&!atomic_load(&progress[PILOT_ERROR]))atomic_store(&progress[PILOT_ERROR],1800+closed);}location_progress();
#endif
    int unhooked=shadowhook_unhook(hook_stub);
    unsigned char after[sizeof(original_bytes)];memcpy(after,(const void *)map.address,sizeof(after));
    if(!unhooked&&!memcmp(after,original_bytes,sizeof(after)))atomic_store(&progress[PILOT_UNHOOKED],1);
    else if(!atomic_load(&progress[PILOT_ERROR]))atomic_store(&progress[PILOT_ERROR],500+unhooked);
    SilphScheduleMetrics metrics={0};
    int snapshot=SILPH_SCHEDULE_STATE;
    if(ready)do {
        snapshot=silph_schedule_metrics(schedule,lease,&metrics);
        if(snapshot==SILPH_SCHEDULE_BUSY){struct timespec pause={0,1000000};nanosleep(&pause,NULL);}
    }while(snapshot==SILPH_SCHEDULE_BUSY&&now_ns()<cleanup_deadline);
    if(!snapshot)atomic_store(&progress[PILOT_MAX_JOB_NS],metrics.max_callback_ns);
    bool execution_ok;
#ifdef SILPH_PILOT_EXTERNAL
    execution_ok=metrics.executed>0&&metrics.executed==atomic_load(&progress[PILOT_JOBS]);
#else
    execution_ok=metrics.executed==SILPH_PILOT_GOAL&&!metrics.expired&&!metrics.canceled&&atomic_load(&progress[PILOT_JOBS])==SILPH_PILOT_GOAL;
#endif
    bool passed=ready&&!snapshot&&metrics.state==SILPH_SCHEDULE_CLOSED&&execution_ok
        &&atomic_load(&progress[PILOT_CLEANUP])==1&&atomic_load(&progress[PILOT_STOPPED])
        &&atomic_load(&progress[PILOT_CREATED])==atomic_load(&progress[PILOT_RELEASED])&&!atomic_load(&progress[PILOT_REMAINING])
        &&atomic_load(&progress[PILOT_UNHOOKED])&&!atomic_load(&progress[PILOT_ERROR]);
    if(!passed&&!atomic_load(&progress[PILOT_ERROR]))atomic_store(&progress[PILOT_ERROR],600);
#ifdef SILPH_PILOT_CONFIGURED
    if(session_link){int closed=silph_session_link_close(session_link);if(closed&&!atomic_load(&progress[PILOT_ERROR]))atomic_store(&progress[PILOT_ERROR],1300+closed);}
#endif
    atomic_store_explicit(&progress[PILOT_STATE],passed?PILOT_COMPLETE:PILOT_FAILED,memory_order_release);return NULL;
}
__attribute__((visibility("default")))
int silph_native_managed_probe(SilphNativeReport *report,uint32_t capacity) {
    int status=silph_native_map_contract_probe(report,capacity);if(status)return status;
#define REJECT(code) do{snprintf(report->detail,sizeof(report->detail),"Unity pilot startup rejected: code=%d errno=%d hookErrno=%d",code,errno,shadowhook_get_errno());return code;}while(0)
    if(atomic_load(&progress[PILOT_MAGIC]))REJECT(10);
    Loaded loaded={.report=report};dl_iterate_phdr(modules,&loaded);
#ifdef SILPH_PILOT_GAME_READ
    game_image=loaded.il2cpp;
#endif
    status=map_contract(&loaded.il2cpp,&map);
    report->managed_created+=map.metrics.refs_created;report->managed_released+=map.metrics.refs_released;
    report->managed_remaining+=map.metrics.live_refs;
    if(!map.metrics.closed)report->managed_closed=0;
    if(status)REJECT(20+status);
    if(silph_managed_api_load(&managed_api,resolve_managed,&loaded.il2cpp))REJECT(11);
    uint64_t nonce=0,ticks=own_ticks();
    if(!ticks||getrandom(&nonce,sizeof(nonce),0)!=sizeof(nonce)||!nonce)REJECT(12);
#ifdef SILPH_PILOT_EXTERNAL
    if(getrandom(&control_key_lo,sizeof(control_key_lo),0)!=sizeof(control_key_lo)||!control_key_lo)REJECT(12);
#endif
    schedule=calloc(1,silph_schedule_size());if(!schedule||silph_schedule_init(schedule,silph_schedule_size()))REJECT(13);
    config=(SilphScheduleConfig){.identity={.pid=(uint32_t)getpid(),.process_ticks=ticks,.session_hi=nonce,.session_lo=1,
        .account_hi=UINT64_C(0x444941474e4f5354),.account_lo=1},.heartbeat_timeout_ns=2200000000,.tick_budget_ns=1000000,
        .max_jobs_per_tick=1,
#if defined(SILPH_PILOT_ACTIONS) && !defined(SILPH_PILOT_READONLY_ONLY)
        .readonly=0,
#else
        .readonly=1,
#endif
        .execute=execute,.cleanup=cleanup};
#ifdef SILPH_PILOT_CONFIGURED
    session_link=calloc(1,silph_session_link_size());
    unsigned mode=
#ifdef SILPH_PILOT_KECLEON
        SILPH_MODE_KECLEON;
#else
        SILPH_MODE_FARMER;
#endif
    if(!session_link||silph_session_link_open(session_link,silph_session_link_size(),&config.identity,mode))REJECT(20);
#endif
    Dl_info info;if(!dladdr((void *)silph_native_managed_probe,&info)||!info.dli_fname)REJECT(14);
    self_pin=dlopen(info.dli_fname,RTLD_NOW|RTLD_NODELETE);
    char backend[4096];const char *slash=strrchr(info.dli_fname,'/');
    if(!self_pin||!slash||snprintf(backend,sizeof(backend),"%.*s/libshadowhook.so",(int)(slash-info.dli_fname),info.dli_fname)>=(int)sizeof(backend))REJECT(15);
    backend_pin=dlopen(backend,RTLD_NOW|RTLD_NODELETE);if(!backend_pin)REJECT(16);
    if(shadowhook_init(SHADOWHOOK_MODE_UNIQUE,true))REJECT(17);
    memcpy(original_bytes,(const void *)map.address,sizeof(original_bytes));
    atomic_store(&progress[PILOT_MAGIC],SILPH_PILOT_MAGIC);atomic_store(&progress[PILOT_ABI],SILPH_PILOT_ABI);
    atomic_store(&progress[PILOT_PID],getpid());atomic_store(&progress[PILOT_TICKS],ticks);atomic_store(&progress[PILOT_NONCE],nonce);
    atomic_store(&progress[PILOT_READONLY],config.readonly);atomic_store(&progress[PILOT_RESIDENT],1);atomic_store(&progress[PILOT_STATE],PILOT_INSTALLING);
#ifdef SILPH_PILOT_EXTERNAL
#endif
    hook_stub=shadowhook_hook_func_addr((void *)map.address,(void *)proxy,(void **)&original);
    if(!hook_stub||!original){atomic_store(&progress[PILOT_ERROR],700+shadowhook_get_errno());REJECT(18);}
    atomic_store_explicit(&enabled,1,memory_order_release);
    pthread_t thread;int created=pthread_create(&thread,NULL,controller,NULL);
    if(created){atomic_store(&enabled,0);shadowhook_unhook(hook_stub);REJECT(19);}
    pthread_detach(thread);
    snprintf(report->detail,sizeof(report->detail),"Unity pilot: address=0x%" PRIxPTR " nonce=%" PRIu64 " resident=1 readOnly=%u",(uintptr_t)progress,nonce,config.readonly);
#ifdef SILPH_PILOT_EXTERNAL
    size_t used=strlen(report->detail);
    snprintf(report->detail+used,sizeof(report->detail)-used," controlKeyLo=%" PRIu64,control_key_lo);
#endif
    return 0;
#undef REJECT
}
__attribute__((visibility("default")))
int silph_native_managed_probe_threaded(SilphNativeReport *report,uint32_t capacity) {
    return silph_startup_thread(silph_native_managed_probe,report,capacity);
}
