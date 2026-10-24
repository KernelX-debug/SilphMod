#include "session-link.h"
#include <stdatomic.h>
#include <string.h>
#define MAGIC UINT32_C(0x534c4e31)
typedef struct {uint32_t magic,mode,configured,head,count;atomic_uint gate;SilphScheduleIdentity identity;SilphSessionConfig config;SilphSessionSnapshot snapshot;
    uint64_t command_sequence;SilphKecleonCommand commands[8];SilphKecleonEvent event;} Link;
size_t silph_session_link_size(void){return sizeof(Link);}
static int lock(Link *s){if(!s||s->magic!=MAGIC)return SILPH_SCHEDULE_ARGUMENT;unsigned zero=0;return atomic_compare_exchange_strong_explicit(&s->gate,&zero,1,memory_order_acquire,memory_order_relaxed)?0:SILPH_SCHEDULE_BUSY;}
static void unlock(Link *s){atomic_store_explicit(&s->gate,0,memory_order_release);}
int silph_session_link_open(void *memory,size_t bytes,const SilphScheduleIdentity *id,uint32_t mode){
    if(!memory||bytes!=sizeof(Link)||(uintptr_t)memory%_Alignof(Link)||!id||id->pid<=1||!id->process_ticks||(!id->session_hi&&!id->session_lo)||mode<1||mode>2)return SILPH_SCHEDULE_ARGUMENT;
    Link *s=memory;if(s->magic==MAGIC)return SILPH_SCHEDULE_STATE;memset(s,0,bytes);atomic_init(&s->gate,0);s->magic=MAGIC;s->mode=mode;s->identity=*id;
    s->snapshot=(SilphSessionSnapshot){.mode=mode,.pid=id->pid,.ticks=id->process_ticks,.session_hi=id->session_hi,.session_lo=id->session_lo};return 0;
}
int silph_session_link_readonly_message(void *memory,const void *bytes,size_t count){
    SilphSessionConfig value;
    if(silph_config_decode(bytes,count,&value)||value.flags!=SILPH_CFG_READONLY)return SILPH_SCHEDULE_DENIED;
    return silph_session_link_message(memory,bytes,count);
}
int silph_session_link_message(void *memory,const void *bytes,size_t count){
    if(count==SILPH_KEC_COMMAND_BYTES){
        Link *s=memory;SilphKecleonCommand command;if(silph_kec_command_decode(bytes,count,&command))return SILPH_SCHEDULE_ARGUMENT;
        int status=lock(s);if(status)return status;
        SilphScheduleIdentity id={.pid=s->snapshot.pid,.process_ticks=s->snapshot.ticks,.session_hi=s->snapshot.session_hi,.session_lo=s->snapshot.session_lo,
            .account_hi=s->snapshot.account_hi,.account_lo=s->snapshot.account_lo};
        if(!s->configured||s->mode!=SILPH_MODE_KECLEON||(s->config.flags&SILPH_CFG_READONLY)||(s->snapshot.flags&8))status=SILPH_SCHEDULE_DENIED;
        else if(memcmp(&id,&command.identity,sizeof(id)))status=SILPH_SCHEDULE_STALE;
        else if(command.sequence!=s->command_sequence+1)status=SILPH_SCHEDULE_ORDER;
        else if(s->count==8)status=SILPH_SCHEDULE_BUSY;
        else{s->commands[(s->head+s->count)%8]=command;s->count++;s->command_sequence=command.sequence;}
        unlock(s);return status;
    }
    Link *s=memory;SilphSessionConfig config;int status=silph_config_decode(bytes,count,&config);if(status)return SILPH_SCHEDULE_ARGUMENT;
    status=lock(s);if(status)return status;
    if(s->configured)status=SILPH_SCHEDULE_STATE;
    else if(config.pid!=s->identity.pid||config.ticks!=s->identity.process_ticks||config.session_hi!=s->identity.session_hi||config.session_lo!=s->identity.session_lo||config.mode!=s->mode)status=SILPH_SCHEDULE_STALE;
    else{s->config=config;s->configured=1;s->snapshot.flags=(config.flags&SILPH_CFG_READONLY)?1:0;s->snapshot.state=1;}
    unlock(s);return status;
}
int silph_session_link_config(void *memory,SilphSessionConfig *out){
    if(!out)return SILPH_SCHEDULE_ARGUMENT;memset(out,0,sizeof(*out));Link *s=memory;int status=lock(s);if(status)return status;
    if(!s->configured)status=SILPH_SCHEDULE_STATE;else *out=s->config;unlock(s);return status;
}
int silph_session_link_publish(void *memory,const SilphSessionSnapshot *next){
    if(!next)return SILPH_SCHEDULE_ARGUMENT;uint8_t bytes[SILPH_SNAPSHOT_BYTES];if(silph_snapshot_encode(next,bytes,sizeof(bytes)))return SILPH_SCHEDULE_ARGUMENT;
    Link *s=memory;int status=lock(s);if(status)return status;
    if(!s->configured||next->pid!=s->identity.pid||next->ticks!=s->identity.process_ticks||next->session_hi!=s->identity.session_hi||next->session_lo!=s->identity.session_lo||next->mode!=s->mode)status=SILPH_SCHEDULE_STALE;
    else if(next->sequence<=s->snapshot.sequence)status=SILPH_SCHEDULE_ORDER;
    else if((s->config.flags&SILPH_CFG_READONLY)&&(!(next->flags&1)||next->caught||next->fled||next->spun||next->discarded||next->transferred||next->encounters||next->berries||next->throws))status=SILPH_SCHEDULE_DENIED;
    else s->snapshot=*next;unlock(s);return status;
}
int silph_session_link_snapshot(void *memory,void *out,size_t capacity,size_t *length){
    if(!length)return SILPH_SCHEDULE_ARGUMENT;*length=0;if(!out||capacity<SILPH_SNAPSHOT_BYTES)return SILPH_SCHEDULE_ARGUMENT;
    Link *s=memory;int status=lock(s);if(status)return status;SilphSessionSnapshot snapshot=s->snapshot;SilphKecleonEvent event=s->event;unlock(s);
    if(silph_snapshot_encode(&snapshot,out,SILPH_SNAPSHOT_BYTES))return SILPH_SCHEDULE_ARGUMENT;*length=SILPH_SNAPSHOT_BYTES;
    if(event.sequence){if(capacity<SILPH_KEC_BUNDLE_BYTES)return SILPH_SCHEDULE_ARGUMENT;if(silph_kec_event_encode(&event,(uint8_t *)out+SILPH_SNAPSHOT_BYTES,SILPH_KEC_EVENT_BYTES))return SILPH_SCHEDULE_ARGUMENT;*length=SILPH_KEC_BUNDLE_BYTES;}return 0;
}
int silph_session_link_pop(void *memory,SilphKecleonCommand *out){
    if(!out)return SILPH_SCHEDULE_ARGUMENT;memset(out,0,sizeof(*out));Link *s=memory;int status=lock(s);if(status)return status;
    if(!s->count)status=SILPH_SCHEDULE_STATE;else{*out=s->commands[s->head];memset(&s->commands[s->head],0,sizeof(*out));s->head=(s->head+1)%8;s->count--;}
    unlock(s);return status;
}
int silph_session_link_event(void *memory,const SilphKecleonEvent *next){
    uint8_t bytes[SILPH_KEC_EVENT_BYTES];if(silph_kec_event_encode(next,bytes,sizeof(bytes)))return SILPH_SCHEDULE_ARGUMENT;
    Link *s=memory;int status=lock(s);if(status)return status;
    if(s->mode!=SILPH_MODE_KECLEON||!s->configured||(s->config.flags&SILPH_CFG_READONLY))status=SILPH_SCHEDULE_DENIED;
    else if(next->sequence<=s->event.sequence)status=SILPH_SCHEDULE_ORDER;
    else if(next->identity.pid!=s->identity.pid||next->identity.process_ticks!=s->identity.process_ticks||next->identity.session_hi!=s->identity.session_hi||next->identity.session_lo!=s->identity.session_lo
        ||next->identity.account_hi!=s->snapshot.account_hi||next->identity.account_lo!=s->snapshot.account_lo)status=SILPH_SCHEDULE_STALE;
    else s->event=*next;unlock(s);return status;
}
int silph_session_link_close(void *memory){Link *s=memory;int status=lock(s);if(status)return status;s->magic=0;memset(&s->config,0,sizeof(s->config));memset(&s->snapshot,0,sizeof(s->snapshot));unlock(s);return 0;}
