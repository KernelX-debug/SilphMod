#define _GNU_SOURCE
#include "protocol.h"
#include "../readback.h"
#include "../core.h"
#include "../managed.h"
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    void *(*domain_get)(void);
    void *(*thread_current)(void);
    void *(*thread_attach)(void *);
    void (*thread_detach)(void *);
    const void **(*assemblies)(void *, size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *, const char *, const char *);
    const void *(*method)(void *, const char *, int);
    uint32_t (*parameter_count)(const void *);
    const void *(*return_type)(const void *);
    int (*type_kind)(const void *);
} Metadata;

typedef struct {
    uintptr_t base;
    struct { uintptr_t begin, end;uint32_t flags; } ranges[32];
    size_t count;
    const Elf64_Dyn *dynamic;
    size_t dynamic_count;
} Image;

typedef struct { SilphNativeReport *report;Image il2cpp, observer; } Loaded;

static int accessible(const Image *image, uintptr_t pointer, size_t bytes, uint32_t flags) {
    if (!pointer || !bytes || pointer > UINTPTR_MAX - bytes) return 0;
    for (size_t index = 0; index < image->count; index++)
        if (pointer >= image->ranges[index].begin && pointer + bytes <= image->ranges[index].end
            && (image->ranges[index].flags & flags) == flags) return 1;
    return 0;
}

static uintptr_t relocated(const Image *image, uintptr_t pointer, size_t bytes) {
    if (accessible(image, pointer, bytes, PF_R)) return pointer;
    if (pointer <= UINTPTR_MAX - image->base && accessible(image, image->base + pointer, bytes, PF_R)) return image->base + pointer;
    return 0;
}

static int image_from(const struct dl_phdr_info *info, Image *image) {
    memset(image, 0, sizeof(*image));image->base = info->dlpi_addr;
    if (!info->dlpi_phdr || !info->dlpi_phnum || info->dlpi_phnum > 256) return 0;
    for (size_t index = 0; index < info->dlpi_phnum; index++) {
        const Elf64_Phdr *part = info->dlpi_phdr + index;
        if (part->p_vaddr > UINTPTR_MAX - image->base) return 0;
        uintptr_t begin = image->base + part->p_vaddr;
        if (part->p_memsz > UINTPTR_MAX - begin) return 0;
        if (part->p_type == PT_LOAD) {
            if (image->count >= 32) return 0;
            image->ranges[image->count].begin = begin;image->ranges[image->count].end = begin + part->p_memsz;
            image->ranges[image->count++].flags = part->p_flags;
        } else if (part->p_type == PT_DYNAMIC && part->p_memsz <= 65536) {
            image->dynamic = (const void *)begin;image->dynamic_count = part->p_memsz / sizeof(Elf64_Dyn);
        }
    }
    return accessible(image, (uintptr_t)image->dynamic, image->dynamic_count * sizeof(Elf64_Dyn), PF_R);
}

static void *export_address(const Image *image, const char *name) {
    uintptr_t symbols = 0, strings = 0, hash = 0, gnu_hash = 0;
    size_t string_bytes = 0, symbol_bytes = 0, count = 0;
    for (size_t index = 0; index < image->dynamic_count; index++) {
        const Elf64_Dyn *entry = image->dynamic + index;
        if (entry->d_tag == DT_NULL) break;
        if (entry->d_tag == DT_SYMTAB) symbols = entry->d_un.d_ptr;
        if (entry->d_tag == DT_STRTAB) strings = entry->d_un.d_ptr;
        if (entry->d_tag == DT_STRSZ) string_bytes = entry->d_un.d_val;
        if (entry->d_tag == DT_SYMENT) symbol_bytes = entry->d_un.d_val;
        if (entry->d_tag == DT_HASH) hash = entry->d_un.d_ptr;
        if (entry->d_tag == DT_GNU_HASH) gnu_hash = entry->d_un.d_ptr;
    }
    if (symbol_bytes != sizeof(Elf64_Sym) || !string_bytes || string_bytes > 64 * 1024 * 1024) return NULL;
    symbols = relocated(image, symbols, sizeof(Elf64_Sym));strings = relocated(image, strings, string_bytes);
    if (!symbols || !strings) return NULL;
    uintptr_t table = relocated(image, hash, 8);
    if (hash && table) count = ((const uint32_t *)table)[1];
    else {
        table = relocated(image, gnu_hash, 16);
        if (!gnu_hash || !table) return NULL;
        const uint32_t *header = (const void *)table;
        uint32_t buckets = header[0], first = header[1], bloom = header[2];
        if (!buckets || buckets > 1048576 || bloom > 1048576 || first > 1048576) return NULL;
        size_t total = 16 + (size_t)bloom * sizeof(Elf64_Addr) + (size_t)buckets * 4;
        if (!accessible(image, table, total, PF_R)) return NULL;
        const uint32_t *bucket = (const void *)(table + 16 + (size_t)bloom * sizeof(Elf64_Addr));
        uint32_t last = 0;for (uint32_t index = 0; index < buckets; index++) if (bucket[index] > last) last = bucket[index];
        if (last < first) count = first;
        else {
            if (last > 1048576) return NULL;
            uintptr_t chains = table + total;
            count = last;
            for (;;) {
                if (count >= 1048576) return NULL;
                uintptr_t current = chains + (count - first) * 4;
                if (!accessible(image, current, 4, PF_R)) return NULL;
                uint32_t end = *(const uint32_t *)current;
                count++;if (end & 1) break;
            }
        }
    }
    if (!count || count > 1048576 || !accessible(image, symbols, count * sizeof(Elf64_Sym), PF_R)) return NULL;
    const Elf64_Sym *entries = (const void *)symbols;
    for (size_t index = 0; index < count; index++) {
        const Elf64_Sym *symbol = entries + index;
        if (symbol->st_shndx == SHN_UNDEF || ELF64_ST_TYPE(symbol->st_info) != STT_FUNC || symbol->st_name >= string_bytes) continue;
        const char *actual = (const void *)(strings + symbol->st_name);
        if (!memchr(actual, 0, string_bytes - symbol->st_name) || strcmp(actual, name)) continue;
        if (symbol->st_value > UINTPTR_MAX - image->base) return NULL;
        uintptr_t address = image->base + symbol->st_value;
        return accessible(image, address, 4, PF_X) ? (void *)address : NULL;
    }
    return NULL;
}

static int modules(struct dl_phdr_info *info, size_t bytes, void *data) {
    (void)bytes;
    Loaded *loaded = data;
    SilphNativeReport *report = loaded->report;
    const char *base = strrchr(info->dlpi_name, '/');
    base = base ? base + 1 : info->dlpi_name;
    if (!strcmp(base, "libil2cpp.so")) { report->il2cpp_loaded = 1;image_from(info, &loaded->il2cpp); }
    if (!strcmp(base, "libunity.so")) report->unity_loaded = 1;
    Image self;
    if (image_from(info, &self) && accessible(&self, (uintptr_t)&modules, 4, PF_X)) loaded->observer = self;
    return 0;
}

static const void *find_image(Metadata *api, const void **assemblies, size_t count, const char *name) {
    for (size_t index = 0; index < count; index++) {
        if (!assemblies[index]) continue;
        const void *image = api->image(assemblies[index]);
        const char *actual = image ? api->image_name(image) : NULL;
        if (actual && !strcmp(actual, name)) return image;
    }
    return NULL;
}

static int method_present(Metadata *api, const void *image, const char *space,
                           const char *type, const char *name, int arguments, int return_kind) {
    void *klass = image ? api->klass(image, space, type) : NULL;
    const void *method = klass ? api->method(klass, name, arguments) : NULL;
    if (!method || api->parameter_count(method) != (uint32_t)arguments) return 0;
    return !return_kind || api->type_kind(api->return_type(method)) == return_kind;
}

static void *resolve_managed(const char *name, void *context) {
    return export_address(context, name);
}
static int managed_probe(SilphNativeReport *report, const Image *image) {
    SilphManagedApi api;
    report->managed_abi = SILPH_MANAGED_ABI;
    int status = silph_managed_api_load(&api, resolve_managed, (void *)image);
    report->managed_closed = 1;report->managed_threads_restored = 1;
    if (status) { report->managed_status = status;return 8; }
    void *previous = api.thread_current();
    size_t bytes = silph_managed_size(8);
    void *state = calloc(1, bytes);
    if (!state) return 8;
    status = silph_managed_open(state, bytes, &api, 8, 64);
    if (status) {
        report->managed_status = status;
        report->managed_threads_restored = api.thread_current() == previous;
        report->managed_closed = status != SILPH_MANAGED_STALE && report->managed_threads_restored;
        free(state);return 8;
    }
    report->managed_started = 1;report->managed_closed = 0;
    SilphManagedRef strong = {0}, weak = {0};SilphManagedMethod getter = {0};
    SilphManagedMetrics metrics = {0};void *value = NULL;int32_t length = 0;
    status = silph_managed_ascii(state, "SilphMod native", 15, &strong);
    if (!status) status = silph_managed_target(state, strong, &value);
    if (!status) status = silph_managed_retain(state, value, true, &weak);
    if (!status) status = silph_managed_target(state, weak, &value);
    if (!status) status = silph_managed_release(state, weak);
    if (!status) status = silph_managed_getter_i32(state, strong, "get_Length", &getter);
    if (!status) status = silph_managed_invoke_i32(state, getter, strong, &length);
    report->managed_value = length;
    int cleanup = silph_managed_close(state, &metrics);
    report->managed_status = status ? status : cleanup;
    report->managed_closed = !cleanup && metrics.closed;
    report->managed_remaining = metrics.live_refs;
    report->managed_owns_attachment = metrics.owns_attachment;
    report->managed_created = metrics.refs_created;report->managed_released = metrics.refs_released;
    report->managed_threads_restored = api.thread_current() == previous;
    report->own_thread_attachment = metrics.owns_attachment;
    report->thread_detached = report->managed_threads_restored;
    if (!cleanup) free(state);
    snprintf(report->detail, sizeof(report->detail), "Managed C bridge: status=%u value=%d refs=%llu/%llu remaining=%u closed=%u threadRestored=%u",
        report->managed_status, length, (unsigned long long)metrics.refs_created, (unsigned long long)metrics.refs_released,
        metrics.live_refs, report->managed_closed, report->managed_threads_restored);
    return status || cleanup || length != 15 || metrics.refs_created != 3 || metrics.refs_released != 3
        || metrics.live_refs || !report->managed_closed || !report->managed_threads_restored ? 8 : 0;
}
static int observe(SilphNativeReport *output, uint32_t capacity, int managed) {
    if (!output || capacity != sizeof(*output)) return 1;
    memset(output, 0, sizeof(*output));
    output->magic = SILPH_NATIVE_MAGIC;
    output->abi = SILPH_NATIVE_ABI;
    output->bytes = sizeof(*output);
    output->pid = (uint32_t)getpid();
    output->page_size = (uint32_t)sysconf(_SC_PAGESIZE);
    output->engine_capabilities = SILPH_NATIVE_CAPABILITIES;
    Loaded loaded;
    memset(&loaded, 0, sizeof(loaded));loaded.report = output;
    dl_iterate_phdr(modules, &loaded);
    uint32_t (*readback)(void) = (void *)export_address(&loaded.observer, "silph_readback_abi");
    if (!readback || readback() != SILPH_READBACK_ABI) {
        snprintf(output->detail, sizeof(output->detail), "Native ELF export lookup could not be verified");return 6;
    }
    output->readback_abi = readback();
    uint32_t (*core_abi)(void) = (void *)export_address(&loaded.observer, "silph_core_abi");
    if (!core_abi || core_abi() != SILPH_CORE_ABI) return 7;
    uint64_t state[64], proof[6] = {0};
    uint32_t bytes = silph_core_size(2);
    if (bytes > sizeof(state) || silph_core_open(state, bytes, 2, 0)) return 7;
    proof[0] = silph_core_issue(state, UINT64_MAX, 1000, 1);
    proof[1] = UINT64_MAX;proof[2] = 1000;proof[3] = 1001;proof[4] = UINT64_MAX - 1;proof[5] = 15;
    int core_verified = proof[0] && silph_core_settle(state, proof, 6) == 1
        && silph_core_authorize(state, UINT64_MAX - 1, UINT64_MAX) == 1
        && silph_core_authorize(state, UINT64_MAX - 1, UINT64_MAX - 1) == 0;
    int core_closed = silph_core_close(state) == 0;
    if (!core_verified || !core_closed) return 7;
    if (!output->il2cpp_loaded) {
        snprintf(output->detail, sizeof(output->detail), "Core ABI 1 receipt checks passed; IL2CPP absent in this process");
        return 0;
    }
    if (!loaded.il2cpp.dynamic || !loaded.il2cpp.dynamic_count) {
        snprintf(output->detail, sizeof(output->detail), "Loaded IL2CPP export metadata is unavailable");
        return 2;
    }
    Metadata api;
    memset(&api, 0, sizeof(api));
#define RESOLVE(member, name) do { \
    *(void **)(&api.member) = export_address(&loaded.il2cpp, name); \
    if (!api.member) goto missing; \
    output->required_exports++; \
} while (0)
    RESOLVE(domain_get, "il2cpp_domain_get");
    RESOLVE(thread_current, "il2cpp_thread_current");
    RESOLVE(thread_attach, "il2cpp_thread_attach");
    RESOLVE(thread_detach, "il2cpp_thread_detach");
    RESOLVE(assemblies, "il2cpp_domain_get_assemblies");
    RESOLVE(image, "il2cpp_assembly_get_image");
    RESOLVE(image_name, "il2cpp_image_get_name");
    RESOLVE(klass, "il2cpp_class_from_name");
    RESOLVE(method, "il2cpp_class_get_method_from_name");
    RESOLVE(parameter_count, "il2cpp_method_get_param_count");
    RESOLVE(return_type, "il2cpp_method_get_return_type");
    RESOLVE(type_kind, "il2cpp_type_get_type");
#undef RESOLVE
    if (!managed) {
        snprintf(output->detail, sizeof(output->detail), "Core ABI 1 receipt checks passed; bootstrap only; managed VM calls deferred");
        return 0;
    }
    if (managed == 2) return managed_probe(output, &loaded.il2cpp);
    void *domain = api.domain_get();
    if (!domain) { snprintf(output->detail, sizeof(output->detail), "IL2CPP domain is not ready"); return 3; }
    void *thread = api.thread_current();
    if (!thread) {
        thread = api.thread_attach(domain);
        if (!thread) { snprintf(output->detail, sizeof(output->detail), "IL2CPP thread attachment failed"); return 4; }
        output->own_thread_attachment = 1;
    }
    size_t count = 0;
    const void **assemblies = api.assemblies(domain, &count);
    if (assemblies && count > 0 && count <= 2048) {
        output->assemblies = (uint32_t)count;
        const void *game = find_image(&api, assemblies, count, "holo-game.dll");
        output->metadata_methods += method_present(&api, game, "Niantic.Holoholo.Map", "MapExploreState", "get_IsActiveAndVisible", 0, 2);
        output->metadata_methods += method_present(&api, game, "Niantic.Holoholo.Map", "MapPokemon", "TryCapture", 2, 0);
        output->metadata_methods += method_present(&api, game, "Niantic.Holoholo", "IItemBag", "GetItemCount", 1, 8);
        output->metadata_methods += method_present(&api, game, "Niantic.Holoholo.Map", "IMapPlaceDirectoryService", "get_AllPokemon", 0, 0);
    }
    if (output->own_thread_attachment) { api.thread_detach(thread); output->thread_detached = 1; }
    else output->thread_detached = 1;
    snprintf(output->detail, sizeof(output->detail), "Core ABI 1 receipt checks passed; metadata only; no game actions");
    return 0;
missing:
    snprintf(output->detail, sizeof(output->detail), "Required IL2CPP export is unavailable");
    return 5;
}
__attribute__((visibility("default")))
int silph_native_probe(SilphNativeReport *output, uint32_t capacity) { return observe(output, capacity, 1); }
__attribute__((visibility("default")))
int silph_native_bootstrap(SilphNativeReport *output, uint32_t capacity) { return observe(output, capacity, 0); }
__attribute__((visibility("default")))
int silph_native_managed_probe(SilphNativeReport *output, uint32_t capacity) { return observe(output, capacity, 2); }
