#ifndef SILPH_NATIVE_PROTOCOL_H
#define SILPH_NATIVE_PROTOCOL_H

#include <stdint.h>

#define SILPH_NATIVE_ABI 2u
#define SILPH_NATIVE_MAGIC UINT32_C(0x53494c50)
#define SILPH_NATIVE_CAPABILITIES 0u

typedef struct {
    uint32_t magic;
    uint32_t abi;
    uint32_t bytes;
    uint32_t pid;
    uint32_t page_size;
    uint32_t il2cpp_loaded;
    uint32_t unity_loaded;
    uint32_t assemblies;
    uint32_t required_exports;
    uint32_t metadata_methods;
    uint32_t own_thread_attachment;
    uint32_t thread_detached;
    uint32_t engine_capabilities;
    uint32_t readback_abi;
    uint32_t managed_abi;
    uint32_t managed_status;
    uint32_t managed_started;
    uint32_t managed_closed;
    uint32_t managed_remaining;
    int32_t managed_value;
    uint32_t managed_owns_attachment;
    uint32_t managed_threads_restored;
    uint64_t managed_created;
    uint64_t managed_released;
    char detail[160];
} SilphNativeReport;

#endif
