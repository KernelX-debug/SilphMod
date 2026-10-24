#ifndef SILPH_ELF_SYMBOL_H
#define SILPH_ELF_SYMBOL_H
#include <stddef.h>
#include <stdint.h>
typedef int (*SilphElfRead)(void *, uint64_t, void *, size_t);
typedef int (*SilphElfExecutable)(void *, uint64_t, uint64_t, uint64_t);
typedef struct {
    SilphElfRead read;
    SilphElfExecutable executable;
    void *context;
    uint64_t bytes, mapping_start, page_size;
    int memory;
    const char *stage;
} SilphElfSource;
int silph_elf_symbol(SilphElfSource *, const char *, uint64_t *, uint64_t *);
#endif
