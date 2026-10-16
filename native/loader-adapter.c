#define _GNU_SOURCE
#include "loader-policy.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <sys/vfs.h>
#include <unistd.h>

typedef int (*SetAttribute)(int, const char *, const void *, size_t, int);
static SetAttribute set_attribute;
static int enabled;
static int context_valid, context_error = ENODEV;
static const char *context_check = "unavailable";
static const char *context_probe = "not_opened";

static void report(const char *stage, int error, int original, int restored) {
    int saved = errno;
    char buffer[512];
    int size = snprintf(buffer, sizeof(buffer),
            "{\"event\":\"host_loader\",\"adapter\":\"readonly-apk-v3\",\"stage\":\"%s\","
            "\"errno\":%d,\"originalErrno\":%d,\"restored\":%s,\"pid\":%d,"
            "\"contextCheck\":\"%s\",\"contextCheckErrno\":%d,\"contextProbe\":\"%s\"}\n",
            stage, error, original, restored ? "true" : "false", getpid(), context_check, context_error, context_probe);
    if (size > 0 && (size_t)size < sizeof(buffer)) {
        ssize_t written;
        do { written = write(STDOUT_FILENO, buffer, (size_t)size); } while (written < 0 && errno == EINTR);
    }
    errno = saved;
}

static void check_context(void) {
    const char *descriptor = getenv("SILPH_LOADER_CONTEXT_FD");
    if (descriptor == NULL || strcmp(descriptor, "9") != 0) return;
    char endpoint[128];
    struct statfs fs;
    ssize_t length = readlink("/proc/self/fd/9", endpoint, sizeof(endpoint) - 1);
    context_probe = "descriptor_unavailable";
    if (length < 0) context_error = errno;
    if (length > 0 && (size_t)length < sizeof(endpoint) - 1) {
        endpoint[length] = 0;
        context_probe = "endpoint_mismatch";
        if (strcmp(endpoint, "/sys/fs/selinux/context") == 0 && fstatfs(9, &fs) == 0
                && ((unsigned long)fs.f_type & 0xffffffffUL) == 0xf97cff8cUL) {
            static const char label[] = "u:object_r:frida_memfd:s0";
            ssize_t written;
            do { written = write(9, label, sizeof(label)); } while (written < 0 && errno == EINTR);
            context_error = written < 0 ? errno : written == (ssize_t)sizeof(label) ? 0 : EIO;
            context_valid = context_error == 0;
            context_check = context_valid ? "valid" : context_error == EINVAL ? "invalid" : "unavailable";
            context_probe = "kernel_query";
        }
    }
    close(9);
}

__attribute__((constructor)) static void prepare(void) {
    const char *scope = getenv("SILPH_LOADER_SCOPE");
    char executable[4096];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length > 0 && (size_t)length < sizeof(executable) - 1) executable[length] = 0;
    else executable[0] = 0;
    const char *name = strrchr(executable, '/');
    enabled = geteuid() == 0 && scope != NULL && strcmp(scope, "com.nianticlabs.pokemongo") == 0
            && name != NULL && (strcmp(name, "/libsilph-spawn.so") == 0
                    || strcmp(name, "/librootcatch-inject.so") == 0);
    set_attribute = (SetAttribute)dlsym(RTLD_NEXT, "fsetxattr");
    if (enabled) check_context();
    unsetenv("LD_PRELOAD");
    unsetenv("SILPH_LOADER_SCOPE");
    unsetenv("SILPH_LOADER_CONTEXT_FD");
    if (enabled) report(set_attribute != NULL ? "ready" : "resolver_failed", set_attribute != NULL ? 0 : ENOSYS, 0, 0);
}

static int label(int fd, const char *context) {
    return set_attribute(fd, "security.selinux", context, strlen(context) + 1, 0);
}
static int context(int fd, char *buffer, size_t capacity) {
    return (int)fgetxattr(fd, "security.selinux", buffer, capacity);
}
static int path(int fd, char *buffer, size_t capacity) {
    char source[64];
    snprintf(source, sizeof(source), "/proc/self/fd/%d", fd);
    return (int)readlink(source, buffer, capacity);
}
static int readonly(int fd) {
    char source[64];
    snprintf(source, sizeof(source), "/proc/self/fd/%d", fd);
    return open(source, O_RDONLY | O_CLOEXEC);
}
static int flags(int fd) { return fcntl(fd, F_GETFD); }
static int replace(int source, int target, int descriptor_flags) {
    int access = fcntl(source, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) != O_RDONLY) { errno = EACCES; return -1; }
    return dup3(source, target, descriptor_flags & FD_CLOEXEC ? O_CLOEXEC : 0) < 0 ? -1 : 0;
}
static void close_fd(int fd) { close(fd); }
static int error(void) { return errno == 0 ? EIO : errno; }

__attribute__((visibility("default"))) int fsetxattr(int fd, const char *name, const void *value, size_t size, int attribute_flags) {
    if (set_attribute == NULL) { errno = ENOSYS; return -1; }
    int result = set_attribute(fd, name, value, size, attribute_flags);
    int original = result == 0 ? 0 : errno;
    if (!enabled || name == NULL || strcmp(name, "security.selinux") != 0 || attribute_flags != 0
            || !silph_loader_label(value, size)) return result;
    const SilphLoaderOps ops = {label, context, path, readonly, flags, replace, close_fd, error};
    SilphLoaderResult adapted = silph_loader_prepare(fd, result, context_valid, &ops);
    if (!adapted.handled) { errno = original; return result; }
    report(adapted.stage, adapted.error, original, adapted.restored);
    errno = adapted.error == 0 ? original : adapted.error;
    return adapted.error == 0 ? 0 : -1;
}
