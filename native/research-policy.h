#ifndef SILPH_RESEARCH_POLICY_H
#define SILPH_RESEARCH_POLICY_H
#include <stdint.h>
#include <stddef.h>
enum {SILPH_RESEARCH_KEEP=0,SILPH_RESEARCH_CLAIM_TASK=1,SILPH_RESEARCH_DISCARD_TASK=2};
int silph_research_decide(uint32_t,uint32_t,uint32_t,uint32_t,const uint32_t *,size_t,uint32_t,uint32_t);
static inline int silph_research_story_context(uint32_t context){return context==1||context==4||context==5||context==11||context==16||context==17;}
static inline int silph_research_encounter_type(uint32_t type){return type==4||type==5||type==6||type==10||type==23||type==25;}
#endif
