#include "research-policy.h"
static int has(const uint32_t *set,size_t count,uint32_t value){for(size_t i=0;i<count;i++)if(set[i]==value)return 1;return 0;}
int silph_research_decide(uint32_t context,uint32_t type,uint32_t status,uint32_t redeemable,const uint32_t *conditions,size_t count,uint32_t transfers,uint32_t berries){
    if(context!=2||redeemable>1||count>32||(count&&!conditions)||transfers>1||berries>1)return SILPH_RESEARCH_KEEP;
    if(redeemable)return SILPH_RESEARCH_CLAIM_TASK;if(status!=1||type==46)return SILPH_RESEARCH_KEEP;
    static const uint32_t supported[]={1,2,3,4,5,6,9,10,12,13,16,17,19,37,41,42,44,45,60,66};
    static const uint32_t valid_conditions[]={1,2,3,4,5,8,11,12,13,14,15,16,17,20,21,24,25,26,28,29,30,31,32,33,34,35,36,37,39,40,42,43,45,46,47,50,51,52,53,54,57,59,63,64,65};
    if(!has(supported,sizeof(supported)/sizeof(*supported),type))return SILPH_RESEARCH_DISCARD_TASK;
    for(size_t i=0;i<count;i++)if(!has(valid_conditions,sizeof(valid_conditions)/sizeof(*valid_conditions),conditions[i]))return SILPH_RESEARCH_DISCARD_TASK;
    if((type==10&&!transfers)||(type==13&&!berries))return SILPH_RESEARCH_DISCARD_TASK;return SILPH_RESEARCH_KEEP;
}
