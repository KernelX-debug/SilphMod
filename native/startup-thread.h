#ifndef SILPH_STARTUP_THREAD_H
#define SILPH_STARTUP_THREAD_H
#include "standalone/protocol.h"
int silph_startup_thread(int (*)(SilphNativeReport *,uint32_t),SilphNativeReport *,uint32_t);
#endif
