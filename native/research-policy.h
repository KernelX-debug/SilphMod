#ifndef SILPH_RESEARCH_POLICY_H
#define SILPH_RESEARCH_POLICY_H
#include <stdint.h>
#include <stddef.h>
enum {SILPH_RESEARCH_KEEP=0,SILPH_RESEARCH_CLAIM_TASK=1,SILPH_RESEARCH_DISCARD_TASK=2};
int silph_research_decide(uint32_t,uint32_t,uint32_t,uint32_t,const uint32_t *,size_t,uint32_t,uint32_t);
#endif
