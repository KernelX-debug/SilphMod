#ifndef SILPH_CODE_MEMORY_H
#define SILPH_CODE_MEMORY_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uintptr_t page;
    uint64_t offset,inode;
    unsigned major,minor;
    int protection;
} SilphCodePage;
typedef struct {
    uintptr_t address;
    unsigned count,dirty_mask,read_method;
    int read_errno,mem_errno,protect_errno,restore_errno;
    unsigned char bytes[16];
    SilphCodePage pages[2];
} SilphCodeMemory;
int silph_code_snapshot(const void *,SilphCodeMemory *);
int silph_code_readable(SilphCodeMemory *);
int silph_code_restore(SilphCodeMemory *);
int silph_code_matches(SilphCodeMemory *);
#endif
