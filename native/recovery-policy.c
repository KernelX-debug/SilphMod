#include "recovery-policy.h"
#include <string.h>
void silph_recovery_pause(SilphRecovery *s){if(s)memset(s,0,sizeof(*s));}
bool silph_recovery_observe(SilphRecovery *s,uint64_t now,uint32_t count,bool valid){
    if(!s)return false;if(!valid||!now||count>100000){silph_recovery_pause(s);return false;}
    if(!s->last||now<s->last||now-s->last>UINT64_C(15000000000))s->empty=0;s->last=now;
    if(count){s->empty=0;return false;}if(!s->empty){s->since=now;s->empty=1;}return now-s->since>=UINT64_C(300000000000);
}
