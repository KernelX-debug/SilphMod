#ifndef SILPH_CALLBACK_LANE_H
#define SILPH_CALLBACK_LANE_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {atomic_uint *depth;} SilphCallbackScope;
static inline SilphCallbackScope silph_callback_enter(atomic_uint *depth){atomic_fetch_add_explicit(depth,1,memory_order_acq_rel);return (SilphCallbackScope){depth};}
static inline void silph_callback_leave(SilphCallbackScope *scope){if(scope->depth){atomic_fetch_sub_explicit(scope->depth,1,memory_order_release);scope->depth=NULL;}}
static inline bool silph_callback_idle(atomic_uint *depth){return atomic_load_explicit(depth,memory_order_acquire)==0;}
#endif
