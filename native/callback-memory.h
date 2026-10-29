#ifndef SILPH_CALLBACK_MEMORY_H
#define SILPH_CALLBACK_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint64_t regs[31],sp,nzcv,fpcr,fpsr,decision,q[64];} SilphCallbackFrame;
bool silph_callback_object(const void *,const void *);
#endif
