#include "kecleon-wire.h"
#include <math.h>
#include <string.h>
static uint32_t g32(const uint8_t *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t g64(const uint8_t *p){return(uint64_t)g32(p)|(uint64_t)g32(p+4)<<32;}
static void p32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void p64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(n>>(8*i));}
static void pd(uint8_t *p,double n){uint64_t v;memcpy(&v,&n,8);p64(p,v);}
static double gd(const uint8_t *p){uint64_t n=g64(p);double v;memcpy(&v,&n,8);return v;}
static int id_valid(const SilphScheduleIdentity *i){return i&&i->pid>1&&i->pid<=INT32_MAX&&i->process_ticks&&i->process_ticks<=INT64_MAX&&(i->session_hi||i->session_lo)&&(i->account_hi||i->account_lo)&&!i->reserved;}
static int coords(double a,double b){return isfinite(a)&&isfinite(b)&&fabs(a)<=90&&fabs(b)<=180;}
static int text(const char *s,size_t n,bool required){
    if(!s||(!s[0]&&required))return 0;size_t end=0;
    for(;end<n&&s[end];end++){unsigned char c=(unsigned char)s[end];if(c<33||c>126)return 0;}
    if(end==n)return 0;for(size_t j=end;j<n;j++)if(s[j])return 0;return 1;
}
static int command_valid(const SilphKecleonCommand *c){
    if(!c||!id_valid(&c->identity)||!c->sequence||c->sequence>INT64_MAX||!c->request||c->request>INT64_MAX||c->kind<1||c->kind>6||c->move>2||c->accepted>1||!coords(c->latitude,c->longitude))return 0;
    if(c->kind==SILPH_KEC_TARGET){
        if(c->move||c->accepted||!text(c->source,97,true))return 0;
        for(unsigned i=0;c->source[i];i++){char ch=c->source[i];if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch=='.'||ch==':'))return 0;}
    }else if(c->latitude||c->longitude||!text(c->source,97,false)||c->source[0])return 0;
    if(c->kind!=SILPH_KEC_MOVED&&c->move)return 0;
    if(c->kind!=SILPH_KEC_MOVED&&c->kind!=SILPH_KEC_AUTHORIZE&&c->accepted)return 0;return text(c->source,97,c->kind==SILPH_KEC_TARGET);
}
static int event_valid(const SilphKecleonEvent *e){
    if(!e||!id_valid(&e->identity)||!e->sequence||e->sequence>INT64_MAX||!e->request||e->request>INT64_MAX||e->phase<1||e->phase>8||e->move>2||e->verified>1||e->reason>10||!coords(e->latitude,e->longitude))return 0;
    bool target=e->phase!=SILPH_KPLAN_NEED_TARGET&&e->phase!=SILPH_KPLAN_FINISHED;
    return text(e->source,97,target)&&text(e->fort,161,e->phase==SILPH_KPLAN_AUTHORIZE||e->phase==SILPH_KPLAN_RUN)
        &&text(e->incident,161,e->phase==SILPH_KPLAN_AUTHORIZE||e->phase==SILPH_KPLAN_RUN);
}
static void identity(uint8_t *p,const SilphScheduleIdentity *i){p32(p+16,i->pid);p64(p+24,i->process_ticks);p64(p+32,i->session_hi);p64(p+40,i->session_lo);p64(p+48,i->account_hi);p64(p+56,i->account_lo);}
static SilphScheduleIdentity read_identity(const uint8_t *p){return(SilphScheduleIdentity){.pid=g32(p+16),.process_ticks=g64(p+24),.session_hi=g64(p+32),.session_lo=g64(p+40),.account_hi=g64(p+48),.account_lo=g64(p+56)};}
static int checked(const uint8_t *p,size_t size,uint64_t magic){return p&&g64(p)==magic&&g32(p+8)==1&&g32(p+12)==size&&g32(p+size-4)==silph_wire_crc(p,size-4);}
int silph_kec_command_encode(const SilphKecleonCommand *c,void *memory,size_t bytes){
    if(!memory||bytes!=SILPH_KEC_COMMAND_BYTES||!command_valid(c))return SILPH_WIRE_ARGUMENT;uint8_t *p=memory;memset(p,0,bytes);
    p64(p,SILPH_KEC_COMMAND_MAGIC);p32(p+8,1);p32(p+12,(uint32_t)bytes);identity(p,&c->identity);p32(p+20,c->kind);p64(p+64,c->sequence);p64(p+72,c->request);
    p32(p+80,c->move);p32(p+84,c->accepted);pd(p+88,c->latitude);pd(p+96,c->longitude);memcpy(p+104,c->source,97);p32(p+508,silph_wire_crc(p,508));return 0;
}
int silph_kec_command_decode(const void *memory,size_t bytes,SilphKecleonCommand *out){
    if(!out)return SILPH_WIRE_ARGUMENT;memset(out,0,sizeof(*out));if(bytes!=SILPH_KEC_COMMAND_BYTES||!checked(memory,bytes,SILPH_KEC_COMMAND_MAGIC))return SILPH_WIRE_FORMAT;
    const uint8_t *p=memory;for(unsigned i=201;i<508;i++)if(p[i])return SILPH_WIRE_FORMAT;
    SilphKecleonCommand c={.identity=read_identity(p),.kind=g32(p+20),.sequence=g64(p+64),.request=g64(p+72),.move=g32(p+80),.accepted=g32(p+84),.latitude=gd(p+88),.longitude=gd(p+96)};
    memcpy(c.source,p+104,97);if(!command_valid(&c))return SILPH_WIRE_ARGUMENT;*out=c;return 0;
}
int silph_kec_event_encode(const SilphKecleonEvent *e,void *memory,size_t bytes){
    if(!memory||bytes!=SILPH_KEC_EVENT_BYTES||!event_valid(e))return SILPH_WIRE_ARGUMENT;uint8_t *p=memory;memset(p,0,bytes);
    p64(p,SILPH_KEC_EVENT_MAGIC);p32(p+8,1);p32(p+12,(uint32_t)bytes);identity(p,&e->identity);p32(p+20,e->phase);p64(p+64,e->sequence);p64(p+72,e->request);
    p32(p+80,e->move);p32(p+84,e->reason);p32(p+88,e->verified);pd(p+96,e->latitude);pd(p+104,e->longitude);
    memcpy(p+112,e->source,97);memcpy(p+209,e->fort,161);memcpy(p+370,e->incident,161);p32(p+636,silph_wire_crc(p,636));return 0;
}
int silph_kec_event_decode(const void *memory,size_t bytes,SilphKecleonEvent *out){
    if(!out)return SILPH_WIRE_ARGUMENT;memset(out,0,sizeof(*out));if(bytes!=SILPH_KEC_EVENT_BYTES||!checked(memory,bytes,SILPH_KEC_EVENT_MAGIC))return SILPH_WIRE_FORMAT;
    const uint8_t *p=memory;for(unsigned i=92;i<96;i++)if(p[i])return SILPH_WIRE_FORMAT;for(unsigned i=531;i<636;i++)if(p[i])return SILPH_WIRE_FORMAT;
    SilphKecleonEvent e={.identity=read_identity(p),.phase=g32(p+20),.sequence=g64(p+64),.request=g64(p+72),.move=g32(p+80),.reason=g32(p+84),.verified=g32(p+88),.latitude=gd(p+96),.longitude=gd(p+104)};
    memcpy(e.source,p+112,97);memcpy(e.fort,p+209,161);memcpy(e.incident,p+370,161);if(!event_valid(&e))return SILPH_WIRE_ARGUMENT;*out=e;return 0;
}
