#include "loader-policy.h"

static const char apk_context[] = "u:object_r:apk_data_file:s0";

static int equal(const char *a, const char *b) {
    if (a == NULL || b == NULL) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

int silph_loader_label(const void *value, size_t size) {
    static const char label[] = "u:object_r:frida_memfd:s0";
    if (value == NULL || (size != sizeof(label) && size != sizeof(label) - 1)) return 0;
    const char *text = value;
    for (size_t i = 0; i < sizeof(label) - 1; i++) if (text[i] != label[i]) return 0;
    return size == sizeof(label) - 1 || text[sizeof(label) - 1] == 0;
}

SilphLoaderResult silph_loader_readonly(int fd, const SilphLoaderOps *ops) {
    SilphLoaderResult result = {0, 0, 0, 0, "unrelated"};
    char path[128], previous[256], effective[256];
    int length = ops->path(fd, path, sizeof(path) - 1);
    if (length < 0 || (size_t)length >= sizeof(path) - 1) return result;
    path[length] = 0;
    if (!equal(path, "/memfd:frida-agent-64.so (deleted)")
            && !equal(path, "memfd:frida-agent-64.so (deleted)")) return result;
    result.handled = 1;
    result.stage = "read_context";
    length = ops->context(fd, previous, sizeof(previous) - 1);
    if (length <= 0 || (size_t)length >= sizeof(previous) - 1) {
        result.error = ops->error(); return result;
    }
    previous[length] = 0;
    result.stage = "read_flags";
    int flags = ops->flags(fd);
    if (flags < 0) { result.error = ops->error(); return result; }
    result.stage = "label_readonly";
    if (ops->label(fd, apk_context) != 0) { result.error = ops->error(); return result; }
    result.stage = "open_readonly";
    int readonly = ops->readonly(fd);
    if (readonly >= 0) {
        result.stage = "verify_context";
        length = ops->context(readonly, effective, sizeof(effective) - 1);
        result.error = ops->error();
        if (length > 0 && (size_t)length < sizeof(effective) - 1) {
            effective[length] = 0;
            if (equal(effective, apk_context)) {
                result.stage = "replace_readonly";
                if (ops->replace(readonly, fd, flags) == 0) result.adapted = 1;
                else result.error = ops->error();
            } else result.error = 5;
        }
        if (result.adapted) result.error = 0;
        ops->close(readonly);
    } else {
        result.error = ops->error();
    }
    if (!result.adapted) result.restored = ops->label(fd, previous) == 0;
    else result.stage = "readonly_apk_memfd";
    return result;
}
