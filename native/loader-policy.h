#ifndef SILPH_LOADER_POLICY_H
#define SILPH_LOADER_POLICY_H

#include <stddef.h>

typedef struct {
    int (*label)(int fd, const char *context);
    int (*context)(int fd, char *buffer, size_t capacity);
    int (*path)(int fd, char *buffer, size_t capacity);
    int (*readonly)(int fd);
    int (*flags)(int fd);
    int (*replace)(int source, int target, int flags);
    void (*close)(int fd);
    int (*error)(void);
} SilphLoaderOps;

typedef struct {
    int handled;
    int adapted;
    int error;
    int restored;
    const char *stage;
} SilphLoaderResult;

int silph_loader_label(const void *value, size_t size);
SilphLoaderResult silph_loader_readonly(int fd, const SilphLoaderOps *ops);
SilphLoaderResult silph_loader_prepare(int fd, int original_result, int context_valid, const SilphLoaderOps *ops);

#endif
