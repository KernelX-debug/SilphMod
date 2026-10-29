#ifndef SILPH_EVENT_POLICY_H
#define SILPH_EVENT_POLICY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SILPH_EVENT_HISTORY 4096u
#define SILPH_EVENT_PENDING 8u
#define SILPH_EVENT_REPEAT_MS 500u
#define SILPH_EVENT_PRIORITY 32u
typedef struct {uint64_t cell,id,until;} SilphEventSeen;
typedef struct {SilphEventSeen seen[SILPH_EVENT_HISTORY];uint32_t head;} SilphEventHistory;
typedef struct {uint64_t cell,id,until;uint32_t index;} SilphEventRareBox;
typedef struct {SilphEventRareBox boxes[SILPH_EVENT_PRIORITY];uint32_t head,cursor;} SilphEventPriority;
static inline SilphEventRareBox *silph_event_rare_find(SilphEventPriority *s,uint64_t cell,uint64_t id){
    for(unsigned i=0;i<SILPH_EVENT_PRIORITY;i++)if(s->boxes[i].id==id&&s->boxes[i].cell==cell)return &s->boxes[i];return NULL;
}
static inline void silph_event_rare_remember(SilphEventPriority *s,uint64_t cell,uint64_t id,uint32_t index,uint64_t until){
    SilphEventRareBox *box=silph_event_rare_find(s,cell,id);if(!box)box=&s->boxes[s->head++%SILPH_EVENT_PRIORITY];*box=(SilphEventRareBox){cell,id,until,index};
}
static inline void silph_event_rare_forget(SilphEventPriority *s,uint64_t cell,uint64_t id){SilphEventRareBox *box=silph_event_rare_find(s,cell,id);if(box)*box=(SilphEventRareBox){0};}
static inline bool silph_event_seen(const SilphEventHistory *s,uint64_t cell,uint64_t id,uint64_t now){
    if(!cell||!id)return true;
    uint32_t start=(uint32_t)(id^(id>>32)^cell)&(SILPH_EVENT_HISTORY-1);
    for(unsigned i=0;i<16;i++){const SilphEventSeen *v=&s->seen[(start+i)&(SILPH_EVENT_HISTORY-1)];if(v->cell==cell&&v->id==id&&v->until>now)return true;}
    return false;
}
static inline void silph_event_remember(SilphEventHistory *s,uint64_t cell,uint64_t id,uint64_t until){
    uint32_t start=(uint32_t)(id^(id>>32)^cell)&(SILPH_EVENT_HISTORY-1),slot=start;
    for(unsigned i=0;i<16;i++){uint32_t n=(start+i)&(SILPH_EVENT_HISTORY-1);SilphEventSeen *v=&s->seen[n];if((v->cell==cell&&v->id==id)||!v->id){slot=n;break;}if(v->until<s->seen[slot].until)slot=n;}
    s->seen[slot]=(SilphEventSeen){cell,id,until};s->head++;
}
static inline uint64_t silph_event_hold(uint64_t now,uint64_t expires){return expires>now&&expires-now<UINT64_C(86400000)?expires:now+UINT64_C(7200000);}
static inline uint64_t silph_event_repeat_hold(uint64_t wall,uint64_t elapsed_ms,uint64_t expires){
    uint64_t delay=elapsed_ms<SILPH_EVENT_REPEAT_MS?SILPH_EVENT_REPEAT_MS-elapsed_ms:100;if(delay<100)delay=100;
    uint64_t next=wall+delay;return expires>wall&&expires<next?expires:next;
}
#endif
