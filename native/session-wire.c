#include "session-wire.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
static uint32_t get32(const uint8_t *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t get64(const uint8_t *p){return(uint64_t)get32(p)|(uint64_t)get32(p+4)<<32;}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(n>>(8*i));}
uint32_t silph_wire_crc(const void *memory,size_t bytes){
    if(!memory&&bytes)return 0;const uint8_t *p=memory;uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<bytes;i++){crc^=p[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(UINT32_C(0xedb88320)&(uint32_t)-(int32_t)(crc&1));}return~crc;
}
static int berry(uint32_t v){return v==701||v==703||v==705||v==706||v==708;}
int silph_config_validate(const SilphSessionConfig *c){
    if(!c||c->pid<=1||c->pid>INT32_MAX||!c->ticks||c->ticks>INT64_MAX||(!c->session_hi&&!c->session_lo)||c->mode<1||c->mode>SILPH_MODE_MAX
#if defined(SILPH_PILOT_EVENT) || defined(SILPH_EVENT_WIRE)
        ||(c->flags&~(c->mode==SILPH_MODE_EVENT?1728u:c->mode==SILPH_MODE_ROCKET?131071u:c->mode==SILPH_MODE_FARMER?196607u:65535u))
#else
        ||(c->flags&~(c->mode==SILPH_MODE_ROCKET?131071u:c->mode==SILPH_MODE_FARMER?196607u:65535u))
#endif
        ||c->quality<1||c->quality>4||c->delay_ms<100||c->delay_ms>3000||c->keep>255||c->max_catches>50000||c->max_spins>50000
        ||c->max_encounters>50000||c->berry_count>5||c->limit_count>64||c->filters>(c->mode==SILPH_MODE_ROCKET?31u:15u)||c->duration_ms>86400000
        ||(c->duration_ms&&c->duration_ms<1000))return SILPH_WIRE_ARGUMENT;
    if(c->flags&SILPH_CFG_READONLY){if(c->flags!=SILPH_CFG_READONLY||c->max_catches||c->max_spins||c->max_encounters||c->filters)return SILPH_WIRE_ARGUMENT;}
#if defined(SILPH_PILOT_EVENT) || defined(SILPH_EVENT_WIRE)
    else if(c->mode==SILPH_MODE_EVENT){
        if(c->max_catches||c->max_spins||c->max_encounters||c->filters||c->keep||c->berry_count||c->limit_count)return SILPH_WIRE_ARGUMENT;
        for(unsigned i=0;i<256;i++)if(c->species[i])return SILPH_WIRE_ARGUMENT;
    }
#endif
    else if(c->mode==SILPH_MODE_FARMER){
        if(!(c->flags&7)||((c->flags&SILPH_CFG_CATCH)&&!c->max_catches)||c->max_encounters||c->filters
            ||(!(c->flags&SILPH_CFG_CATCH)&&(c->flags&(SILPH_CFG_TRANSFER|SILPH_CFG_BERRIES))))return SILPH_WIRE_ARGUMENT;
    }else if(!(c->flags&SILPH_CFG_CATCH)||(c->flags&(SILPH_CFG_SPIN|SILPH_CFG_TRANSFER))||!c->max_encounters||c->max_catches||c->max_spins)return SILPH_WIRE_ARGUMENT;
    if(((c->flags&SILPH_CFG_BERRIES)!=0)!=(c->berry_count!=0)||((c->flags&SILPH_CFG_CLEAN)!=0)!=(c->limit_count!=0))return SILPH_WIRE_ARGUMENT;
    for(unsigned i=0;i<5;i++){
        if(i>=c->berry_count){if(c->berries[i])return SILPH_WIRE_ARGUMENT;continue;}
        if(!berry(c->berries[i]))return SILPH_WIRE_ARGUMENT;for(unsigned j=0;j<i;j++)if(c->berries[j]==c->berries[i])return SILPH_WIRE_ARGUMENT;
    }
    for(unsigned i=0;i<64;i++){
        if(i>=c->limit_count){if(c->limits[i].item||c->limits[i].floor)return SILPH_WIRE_ARGUMENT;continue;}
        if(!c->limits[i].item||c->limits[i].item==4||c->limits[i].item>100000||c->limits[i].floor>100000)return SILPH_WIRE_ARGUMENT;
        for(unsigned j=0;j<i;j++)if(c->limits[j].item==c->limits[i].item)return SILPH_WIRE_ARGUMENT;
    }
    if(c->species[0]&1)return SILPH_WIRE_ARGUMENT;
    if(c->mode==SILPH_MODE_KECLEON)for(unsigned i=0;i<256;i++)if(c->species[i]&&(i!=44||c->species[i]!=1))return SILPH_WIRE_ARGUMENT;
#ifdef SILPH_PILOT_ROCKET
    if((c->potion_count&&c->potion_count!=4)||(c->revive_count&&c->revive_count!=2))return SILPH_WIRE_ARGUMENT;
    if((c->potion_count||c->revive_count)&&(c->mode!=SILPH_MODE_ROCKET||(c->flags&SILPH_CFG_READONLY)))return SILPH_WIRE_ARGUMENT;
    for(unsigned i=0;i<4;i++){uint32_t id=c->potion_order[i];if(i>=c->potion_count){if(id)return SILPH_WIRE_ARGUMENT;continue;}if(id<101||id>104)return SILPH_WIRE_ARGUMENT;for(unsigned j=0;j<i;j++)if(id==c->potion_order[j])return SILPH_WIRE_ARGUMENT;}
    for(unsigned i=0;i<2;i++){uint32_t id=c->revive_order[i];if(i>=c->revive_count){if(id)return SILPH_WIRE_ARGUMENT;continue;}if(id<201||id>202)return SILPH_WIRE_ARGUMENT;for(unsigned j=0;j<i;j++)if(id==c->revive_order[j])return SILPH_WIRE_ARGUMENT;}
#endif
    #ifdef SILPH_PRESENT_SCOPED
    if(c->buddy_flags>3||c->buddy_berry_count>5||(c->keep_species[0]&1))return SILPH_WIRE_ARGUMENT;
    bool extra=c->buddy_flags||c->buddy_berry_count;
    for(unsigned i=0;i<136;i++)if(c->keep_species[i])extra=true;
    if(extra&&(c->mode!=SILPH_MODE_FARMER||(c->flags&SILPH_CFG_READONLY)))return SILPH_WIRE_ARGUMENT;
    if(!!(c->buddy_flags&1)!=!!c->buddy_berry_count)return SILPH_WIRE_ARGUMENT;
    for(unsigned i=0;i<5;i++){
        if(i>=c->buddy_berry_count){if(c->buddy_berries[i])return SILPH_WIRE_ARGUMENT;continue;}
        if(!berry(c->buddy_berries[i]))return SILPH_WIRE_ARGUMENT;
        for(unsigned j=0;j<i;j++)if(c->buddy_berries[i]==c->buddy_berries[j])return SILPH_WIRE_ARGUMENT;
    }
#endif
    return SILPH_WIRE_OK;
}
int silph_config_encode(const SilphSessionConfig *c,void *memory,size_t bytes){
    if(!memory||bytes!=SILPH_CONFIG_BYTES||silph_config_validate(c))return SILPH_WIRE_ARGUMENT;uint8_t *p=memory;memset(p,0,bytes);
    put64(p,SILPH_CONFIG_MAGIC);put32(p+8,1);put32(p+12,(uint32_t)bytes);put32(p+16,c->pid);put32(p+20,c->mode);
    put64(p+24,c->ticks);put64(p+32,c->session_hi);put64(p+40,c->session_lo);
    const uint32_t scalars[]={c->flags,c->quality,c->delay_ms,c->keep,c->max_catches,c->max_spins,c->max_encounters,c->berry_count};
    for(unsigned i=0;i<8;i++)put32(p+48+4*i,scalars[i]);for(unsigned i=0;i<5;i++)put32(p+80+4*i,c->berries[i]);put32(p+100,c->limit_count);
    for(unsigned i=0;i<64;i++){put32(p+104+i*8,c->limits[i].item);put32(p+108+i*8,c->limits[i].floor);}
    memcpy(p+616,c->species,256);put32(p+872,c->filters);put32(p+876,c->duration_ms);
#ifdef SILPH_PILOT_ROCKET
    p[880]=(uint8_t)c->potion_count;for(unsigned i=0;i<4;i++)p[881+i]=(uint8_t)c->potion_order[i];p[885]=(uint8_t)c->revive_count;for(unsigned i=0;i<2;i++)p[886+i]=(uint8_t)c->revive_order[i];
#endif
#ifdef SILPH_PRESENT_SCOPED
    memcpy(p+880,c->keep_species,136);p[1016]=(uint8_t)c->buddy_flags;uint32_t packed=c->buddy_berry_count;
    const uint32_t ids[]={701,703,705,706,708};
    for(unsigned i=0;i<c->buddy_berry_count;i++)for(unsigned j=0;j<5;j++)if(ids[j]==c->buddy_berries[i])packed|=(j+1)<<(3+3*i);
    for(unsigned i=0;i<3;i++)p[1017+i]=(uint8_t)(packed>>(8*i));
#endif
    put32(p+1020,silph_wire_crc(p,1020));return 0;
}
int silph_config_decode(const void *memory,size_t bytes,SilphSessionConfig *out){
    if(!out)return SILPH_WIRE_ARGUMENT;memset(out,0,sizeof(*out));if(!memory||bytes!=SILPH_CONFIG_BYTES)return SILPH_WIRE_FORMAT;const uint8_t *p=memory;
    if(get64(p)!=SILPH_CONFIG_MAGIC||get32(p+8)!=1||get32(p+12)!=bytes)return SILPH_WIRE_FORMAT;
    if(get32(p+1020)!=silph_wire_crc(p,1020))return SILPH_WIRE_CRC;
#ifndef SILPH_PRESENT_SCOPED
#ifdef SILPH_PILOT_ROCKET
    for(unsigned i=888;i<1020;i++)if(p[i])return SILPH_WIRE_FORMAT;
#else
    for(unsigned i=880;i<1020;i++)if(p[i])return SILPH_WIRE_FORMAT;
#endif
#endif
    SilphSessionConfig c={.pid=get32(p+16),.mode=get32(p+20),.ticks=get64(p+24),.session_hi=get64(p+32),.session_lo=get64(p+40)};
    uint32_t *scalars[]={&c.flags,&c.quality,&c.delay_ms,&c.keep,&c.max_catches,&c.max_spins,&c.max_encounters,&c.berry_count};
    for(unsigned i=0;i<8;i++)*scalars[i]=get32(p+48+4*i);for(unsigned i=0;i<5;i++)c.berries[i]=get32(p+80+4*i);c.limit_count=get32(p+100);
    for(unsigned i=0;i<64;i++){c.limits[i].item=get32(p+104+i*8);c.limits[i].floor=get32(p+108+i*8);}
    memcpy(c.species,p+616,256);c.filters=get32(p+872);c.duration_ms=get32(p+876);
#ifdef SILPH_PILOT_ROCKET
    c.potion_count=p[880];for(unsigned i=0;i<4;i++)c.potion_order[i]=p[881+i];c.revive_count=p[885];for(unsigned i=0;i<2;i++)c.revive_order[i]=p[886+i];
#endif
    #ifdef SILPH_PRESENT_SCOPED
    memcpy(c.keep_species,p+880,136);c.buddy_flags=p[1016];uint32_t packed=(uint32_t)p[1017]|(uint32_t)p[1018]<<8|(uint32_t)p[1019]<<16;
    c.buddy_berry_count=packed&7;if(c.buddy_berry_count>5||(packed>>(3+3*c.buddy_berry_count)))return SILPH_WIRE_FORMAT;
    const uint32_t ids[]={701,703,705,706,708};for(unsigned i=0;i<c.buddy_berry_count;i++){uint32_t code=(packed>>(3+3*i))&7;if(code<1||code>5)return SILPH_WIRE_FORMAT;c.buddy_berries[i]=ids[code-1];}
#endif
    if(silph_config_validate(&c))return SILPH_WIRE_ARGUMENT;*out=c;return 0;
}
static int snapshot_valid(const SilphSessionSnapshot *s){
    return s&&s->pid>1&&s->pid<=INT32_MAX&&s->ticks&&s->ticks<=INT64_MAX&&(s->session_hi||s->session_lo)&&s->mode>=1&&s->mode<=SILPH_MODE_MAX&&s->state<=7&&s->flags<=15
        &&s->rewards_available<=3&&s->research_status<=65535&&s->location_status<=65535&&!(s->visual_available&~7552u)&&!(s->visual_failed&~7552u)&&!(s->visual_available&s->visual_failed)
        &&isfinite(s->latitude)&&isfinite(s->longitude)&&fabs(s->latitude)<=90&&fabs(s->longitude)<=180;
}
int silph_snapshot_encode(const SilphSessionSnapshot *s,void *memory,size_t bytes){
    if(!memory||bytes!=SILPH_SNAPSHOT_BYTES||!snapshot_valid(s))return SILPH_WIRE_ARGUMENT;uint8_t *p=memory;memset(p,0,bytes);
    put64(p,SILPH_SNAPSHOT_MAGIC);put32(p+8,1);put32(p+12,(uint32_t)bytes);
    const uint32_t a[]={s->mode,s->state,s->error,s->reason,s->flags,s->pid};for(unsigned i=0;i<6;i++)put32(p+16+i*4,a[i]);
    const uint64_t b[]={s->ticks,s->session_hi,s->session_lo,s->sequence,s->account_hi,s->account_lo,s->caught,s->fled,s->spun,s->discarded,s->transferred,
        s->encounters,s->shinies,s->berries,s->throws,s->xp,s->stardust,s->encounter,s->captured,s->candidate_request};
    for(unsigned i=0;i<20;i++)put64(p+40+i*8,b[i]);uint64_t lat=0,lon=0;memcpy(&lat,&s->latitude,8);memcpy(&lon,&s->longitude,8);
    put64(p+200,lat);put64(p+208,lon);put32(p+216,s->candidate_state);put32(p+220,s->rewards_available);
    put32(p+224,s->visual_available);put32(p+228,s->visual_failed);put64(p+232,s->iv_reads);put64(p+240,s->name_updates);put32(p+248,s->research_status|(s->location_status<<16));put32(p+252,silph_wire_crc(p,252));return 0;
}
int silph_snapshot_decode(const void *memory,size_t bytes,SilphSessionSnapshot *out){
    if(!out)return SILPH_WIRE_ARGUMENT;memset(out,0,sizeof(*out));if(!memory||bytes!=SILPH_SNAPSHOT_BYTES)return SILPH_WIRE_FORMAT;const uint8_t *p=memory;
    if(get64(p)!=SILPH_SNAPSHOT_MAGIC||get32(p+8)!=1||get32(p+12)!=bytes)return SILPH_WIRE_FORMAT;if(get32(p+252)!=silph_wire_crc(p,252))return SILPH_WIRE_CRC;
    SilphSessionSnapshot s={0};
    uint32_t *a[]={&s.mode,&s.state,&s.error,&s.reason,&s.flags,&s.pid};for(unsigned i=0;i<6;i++)*a[i]=get32(p+16+i*4);
    uint64_t *b[]={&s.ticks,&s.session_hi,&s.session_lo,&s.sequence,&s.account_hi,&s.account_lo,&s.caught,&s.fled,&s.spun,&s.discarded,&s.transferred,
        &s.encounters,&s.shinies,&s.berries,&s.throws,&s.xp,&s.stardust,&s.encounter,&s.captured,&s.candidate_request};
    for(unsigned i=0;i<20;i++)*b[i]=get64(p+40+i*8);uint64_t lat=get64(p+200),lon=get64(p+208);memcpy(&s.latitude,&lat,8);memcpy(&s.longitude,&lon,8);
    s.candidate_state=get32(p+216);s.rewards_available=get32(p+220);s.visual_available=get32(p+224);s.visual_failed=get32(p+228);s.iv_reads=get64(p+232);s.name_updates=get64(p+240);s.research_status=get32(p+248)&65535u;s.location_status=get32(p+248)>>16;
    if(!snapshot_valid(&s))return SILPH_WIRE_ARGUMENT;*out=s;return 0;
}
