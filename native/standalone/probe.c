#define _GNU_SOURCE
#include "protocol.h"
#include "../observer-identity.h"
#include "remote.h"
#include <android/dlext.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint64_t mmap, munmap, open, close, error_address, getpid;
    uint64_t dlopen, dlclose, dlsym, dlerror, android_dlopen;
} Symbols;

static void json_string(const char *value) {
    putchar('"');
    for (const unsigned char *at = (const void *)value; *at; at++) {
        if (*at == '"' || *at == '\\') { putchar('\\');putchar(*at); }
        else if (*at < 32) printf("\\u%04x", *at);
        else putchar(*at);
    }
    putchar('"');
}

static void event(const char *stage, int error) {
    printf("{\"event\":\"native_loader_stage\",\"stage\":");json_string(stage);
    printf(",\"errno\":%d,\"runtime\":\"NATIVE_C\",\"gameActions\":0}\n", error);fflush(stdout);
}

static int resolve(pid_t pid, Symbols *symbols) {
#define RESOLVE(member, module, name) do { if (silph_remote_symbol(pid, module, name, &symbols->member)) { event("resolve:" name, errno);return -1; } } while (0)
    RESOLVE(mmap, "libc.so", "mmap");
    RESOLVE(munmap, "libc.so", "munmap");
    RESOLVE(open, "libc.so", "open");
    RESOLVE(close, "libc.so", "close");
    RESOLVE(error_address, "libc.so", "__errno");
    RESOLVE(getpid, "libc.so", "getpid");
    RESOLVE(dlopen, "libdl.so", "dlopen");
    RESOLVE(dlclose, "libdl.so", "dlclose");
    RESOLVE(dlsym, "libdl.so", "dlsym");
    RESOLVE(dlerror, "libdl.so", "dlerror");
    RESOLVE(android_dlopen, "libdl.so", "android_dlopen_ext");
#undef RESOLVE
    return 0;
}

static int call(SilphTrace *trace, const char *stage, uint64_t function,
                 const uint64_t *arguments, size_t count, uint64_t *result, int borrowed) {
    trace->stage = stage;
    int status = silph_trace_call(trace, function, arguments, count, result, borrowed);
    event(stage, status ? errno : 0);return status;
}

static int library_valid(const char *path) {
    const char *base = strrchr(path, '/');
    if (!base || strstr(path, "/../") || strlen(path) >= PATH_MAX) return 0;
    if (!silph_observer_name_valid(base + 1)) return 0;
    if (strncmp(path, "/data/app/", 10)) return 0;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    Elf64_Ehdr header;
    ssize_t count = read(fd, &header, sizeof(header));close(fd);
    return count == sizeof(header) && !memcmp(header.e_ident, ELFMAG, SELFMAG)
        && header.e_ident[EI_CLASS] == ELFCLASS64 && header.e_machine == EM_AARCH64 && header.e_type == ET_DYN;
}

static void remote_error(SilphTrace *trace, Symbols *symbols, char *output, size_t capacity) {
    uint64_t address = 0;
    if (call(trace, "dlerror", symbols->dlerror, NULL, 0, &address, 0) || !address) return;
    size_t index = 0;
    for (; index + 1 < capacity; index++) {
        if (silph_remote_read(trace->pid, address + index, output + index, 1)) break;
        if (!output[index]) return;
    }
    output[index] = 0;
}

static int run(pid_t pid, uint64_t ticks, const char *library_path, const char *route, int seized, int managed, uint32_t appearance) {
    Symbols symbols;
    memset(&symbols, 0, sizeof(symbols));
    SilphTrace trace;
    memset(&trace, 0, sizeof(trace));
    SilphNativeReport report;
    memset(&report, 0, sizeof(report));
    uint64_t allocation = 0, handle = 0, error_pointer = 0, result = 0;
    int original_errno = 0, errno_saved = 0, remote_fd = -1;
    int passed = 0, unmap_confirmed = 0, unload_confirmed = 0, fd_closed = 1;
    const uint64_t allocation_bytes = 262144;
    const char *failure = "SYMBOL_RESOLUTION", *failed_stage = "resolve_symbols";
    int failed_errno = 0;
    char detail[384] = "";
    if (silph_process_check(pid, ticks, NULL)) { failed_errno = errno;goto finish; }
    event("resolve_target_elf", 0);
    if (resolve(pid, &symbols)) { failed_errno = errno;goto finish; }
    failure = "ATTACH";
    if (silph_trace_open_mode(&trace, pid, ticks, seized)) { failed_stage = trace.stage;failed_errno = trace.error;goto cleanup; }
    uint64_t args[6] = {0};
    failure = "REMOTE_CALL";
    if (call(&trace, "verify_remote_pid", symbols.getpid, NULL, 0, &result, 1)) goto call_failed;
    if (result != (uint64_t)pid) { failure = "IDENTITY";failed_stage = "remote_pid_mismatch";goto cleanup; }
    if (call(&trace, "save_errno", symbols.error_address, NULL, 0, &error_pointer, 1)) goto call_failed;
    if (!error_pointer || silph_remote_read(pid, error_pointer, &original_errno, sizeof(original_errno))) goto call_failed;
    errno_saved = 1;
    args[0] = 0;args[1] = allocation_bytes;args[2] = PROT_READ | PROT_WRITE;
    args[3] = MAP_PRIVATE | MAP_ANONYMOUS;args[4] = UINT64_MAX;args[5] = 0;
    if (call(&trace, "allocate_scratch", symbols.mmap, args, 6, &allocation, 1)) goto call_failed;
    if (!allocation || allocation == UINT64_MAX) { failure = "ALLOCATION";failed_stage = "remote_mmap_failed";goto cleanup; }
    trace.stack = (allocation + allocation_bytes - 256) & ~UINT64_C(15);
    const char *symbol = managed == 3 ? "silph_native_appearance_bootstrap" : managed == 2 ? "silph_native_managed_probe_threaded" : managed ? "silph_native_probe" : "silph_native_bootstrap";
    if(managed==2){printf("{\"event\":\"native_managed_profile\",\"method\":\"%s\"}\n","pthread_stack");fflush(stdout);}
    if (silph_remote_write(pid, allocation, library_path, strlen(library_path) + 1)
        || silph_remote_write(pid, allocation + 4096, symbol, strlen(symbol) + 1)) goto call_failed;
    failure = "LINKER";
    if (!strcmp(route, "fd")) {
        args[0] = allocation;args[1] = O_RDONLY | O_CLOEXEC;
        if (call(&trace, "open_library_fd", symbols.open, args, 2, &result, 0)) goto call_failed;
        remote_fd = (int32_t)result;
        if (remote_fd < 0) { failed_stage = "library_fd_denied";goto cleanup; }
        fd_closed = 0;
        android_dlextinfo ext;
        memset(&ext, 0, sizeof(ext));ext.flags = ANDROID_DLEXT_USE_LIBRARY_FD;ext.library_fd = remote_fd;
        if (silph_remote_write(pid, allocation + 6144, &ext, sizeof(ext))) goto call_failed;
        args[0] = allocation;args[1] = RTLD_NOW;args[2] = allocation + 6144;
        if (call(&trace, "android_dlopen_ext", symbols.android_dlopen, args, 3, &handle, 0)) goto call_failed;
    } else {
        args[0] = allocation;args[1] = RTLD_NOW;
        if (call(&trace, "dlopen", symbols.dlopen, args, 2, &handle, 0)) goto call_failed;
    }
    if (!handle) { failed_stage = "library_load_rejected";remote_error(&trace, &symbols, detail, sizeof(detail));goto cleanup; }
    args[0] = handle;args[1] = allocation + 4096;
    if (call(&trace, "resolve_probe_entry", symbols.dlsym, args, 2, &result, 0)) goto call_failed;
    if (!result) { failed_stage = "entry_not_found";remote_error(&trace, &symbols, detail, sizeof(detail));goto cleanup; }
    const uint64_t entry = result;
    args[0] = allocation + 8192;args[1] = sizeof(report);args[2] = appearance;
    failure = "REMOTE_CALL";
    if (call(&trace, managed==3 ? "native_appearance_bootstrap" : managed == 2 ? "native_managed_bridge_probe" : managed ? "native_metadata_probe" : "native_bootstrap_probe", entry, args, managed==3?3:2, &result, 0)) goto call_failed;
    if (silph_remote_read(pid, allocation + 8192, &report, sizeof(report))) goto call_failed;
    failure = "PROTOCOL";
    if (managed == 2 && report.managed_abi == 1 && (!report.managed_closed || !report.managed_threads_restored)) {
        trace.incomplete_call = 1;failure = "CLEANUP";failed_stage = "managed_cleanup_unconfirmed";goto cleanup;
    }
    if(result){failure="INITIALIZATION";failed_stage="native_initialization_rejected";goto cleanup;}
    if (report.magic != SILPH_NATIVE_MAGIC || report.abi != SILPH_NATIVE_ABI || report.bytes != sizeof(report)
        || report.pid != (uint32_t)pid || report.page_size != (uint32_t)sysconf(_SC_PAGESIZE)
        || report.engine_capabilities != SILPH_NATIVE_CAPABILITIES) {
        failed_stage = "probe_contract_not_validated";goto cleanup;
    }
    passed = 1;failure = "NONE";failed_stage = "complete";goto cleanup;
call_failed:
    failed_stage = trace.stage ? trace.stage : "remote_memory";failed_errno = errno;
cleanup:
    if (trace.attached && trace.stopped && !trace.incomplete_call) {
        uint64_t discard = 0, cleanup_args[2];
        if (handle) {
            cleanup_args[0] = handle;
            unload_confirmed = !call(&trace, "dlclose", symbols.dlclose, cleanup_args, 1, &discard, 0) && (int32_t)discard == 0;
            if (!unload_confirmed) passed = 0;
        } else unload_confirmed = 1;
        if (remote_fd >= 0 && !trace.incomplete_call) {
            cleanup_args[0] = (uint64_t)remote_fd;
            fd_closed = !call(&trace, "close_library_fd", symbols.close, cleanup_args, 1, &discard, 0) && (int32_t)discard == 0;
            if (!fd_closed) passed = 0;
        }
        if (allocation && allocation != UINT64_MAX && !trace.incomplete_call) {
            cleanup_args[0] = allocation;cleanup_args[1] = allocation_bytes;
            unmap_confirmed = !call(&trace, "release_scratch", symbols.munmap, cleanup_args, 2, &discard, 1) && (int32_t)discard == 0;
            if (!unmap_confirmed) passed = 0;
        } else if (!allocation) unmap_confirmed = 1;
        if (errno_saved && !trace.incomplete_call && silph_remote_write(pid, error_pointer, &original_errno, sizeof(original_errno))) passed = 0;
    }
    if (silph_trace_close(&trace)) passed = 0;
    if (!trace.restored || !trace.detached) passed = 0;
    if (!passed && !strcmp(failure, "NONE")) { failure = "CLEANUP";failed_stage = trace.stage; }
finish:
    if (!trace.attached && !trace.registers_dirty) { trace.restored = 1;trace.detached = 1; }
    if (!handle) unload_confirmed = 1;
    if (!allocation) unmap_confirmed = 1;
    if (!passed && !failed_errno && trace.error) failed_errno = trace.error;
    char target_state = 0;
    int target_alive = !silph_process_check(pid, ticks, &target_state);
    if (!target_alive) {
        passed = 0;failure = "TARGET_EXIT";failed_stage = "target_process_exited";failed_errno = ESRCH;
    }
    if (!passed && (failed_errno == EACCES || failed_errno == EPERM)) failure = "POLICY";
    printf("{\"event\":\"native_loader_result\",\"abi\":%u,\"passed\":%s,\"route\":", SILPH_NATIVE_ABI, passed ? "true" : "false");
    json_string(route);printf(",\"failure\":");json_string(failure);
    printf(",\"stage\":");json_string(failed_stage);
    printf(",\"errno\":%d,\"targetAlive\":%s,\"targetState\":\"%c\",\"detail\":", failed_errno, target_alive ? "true" : "false", target_state ? target_state : '?');json_string(detail);
    printf(",\"pid\":%d,\"startTicks\":\"%" PRIu64 "\",\"trace\":\"%s\",\"controlledReturnStops\":%d,\"quiescentSyscall\":%d,"
           "\"restored\":%s,\"detached\":%s,\"libraryReleased\":%s,\"scratchReleased\":%s,\"fdClosed\":%s,\"restartRequired\":%s,"
           "\"runtime\":\"NATIVE_C\",\"engineCapabilities\":0,\"gameActions\":0,\"observation\":{"
           "\"abi\":%u,\"bytes\":%u,\"il2cppLoaded\":%u,\"unityLoaded\":%u,\"assemblies\":%u,\"exports\":%u,\"methods\":%u,"
           "\"ownsThreadAttachment\":%u,\"threadDetached\":%u,\"pageSize\":%u,\"readbackAbi\":%u,\"detail\":",
           pid, ticks, seized ? "PTRACE_SEIZE" : "PTRACE_ATTACH", trace.controlled_returns, trace.quiescent_syscall, trace.restored ? "true" : "false", trace.detached ? "true" : "false",
           unload_confirmed ? "true" : "false", unmap_confirmed ? "true" : "false", fd_closed ? "true" : "false", trace.incomplete_call || !trace.restored ? "true" : "false",
           report.abi, report.bytes, report.il2cpp_loaded, report.unity_loaded, report.assemblies, report.required_exports, report.metadata_methods,
           report.own_thread_attachment, report.thread_detached, report.page_size, report.readback_abi);
    report.detail[sizeof(report.detail) - 1] = 0;json_string(report.detail);
    printf("},\"managed\":{\"abi\":%u,\"status\":%u,\"started\":%u,\"closed\":%u,\"remaining\":%u,\"value\":%d,"
        "\"ownsAttachment\":%u,\"threadRestored\":%u,\"created\":%" PRIu64 ",\"released\":%" PRIu64 "},",
        report.managed_abi, report.managed_status, report.managed_started, report.managed_closed, report.managed_remaining,
        report.managed_value, report.managed_owns_attachment, report.managed_threads_restored, report.managed_created, report.managed_released);
    printf("\"fault\":{\"signal\":%d,\"code\":%d,\"pc\":\"0x%" PRIx64 "\",\"address\":\"0x%" PRIx64 "\",\"sp\":\"0x%" PRIx64 "\"}}\n",
        trace.fault_signal, trace.fault_code, trace.fault_pc, trace.fault_address, trace.fault_sp);fflush(stdout);
    return passed ? 0 : 1;
}

static uint64_t decimal(const char *value) {
    if (!value || !*value || strspn(value, "0123456789") != strlen(value)) return 0;
    errno = 0;char *end = NULL;uint64_t number = strtoull(value, &end, 10);
    return !errno && end && !*end ? number : 0;
}

int main(int argc, char **argv) {
    void *local_loader = dlopen(NULL, RTLD_NOW);
    if (local_loader) dlclose(local_loader);
    const char *library = NULL, *route = "path";
    pid_t pid = 0;
    uint64_t ticks = 0;
    int seized = 1;
    int managed = 0;uint32_t appearance=0;
    for (int index = 1; index < argc; index++) {
        if (index + 1 < argc && !strcmp(argv[index], "--library")) library = argv[++index];
        else if (index + 1 < argc && !strcmp(argv[index], "--route")) route = argv[++index];
        else if (index + 1 < argc && !strcmp(argv[index], "--trace")) {
            const char *mode = argv[++index];
            if (strcmp(mode, "seize") && strcmp(mode, "attach")) return 2;
            seized = !strcmp(mode, "seize");
        }
        else if (index + 1 < argc && !strcmp(argv[index], "--level")) {
            const char *level = argv[++index];
            if (strcmp(level, "bootstrap") && strcmp(level, "metadata") && strcmp(level, "managed") && strcmp(level,"appearance")) return 2;
            managed = !strcmp(level,"appearance") ? 3 : !strcmp(level, "managed") ? 2 : !strcmp(level, "metadata");
        }
        else if (index + 1 < argc && !strcmp(argv[index], "--features")) {uint64_t number=decimal(argv[++index]);if(number>UINT32_MAX)return 2;appearance=(uint32_t)number;}
        else if (index + 1 < argc && !strcmp(argv[index], "--pid")) { uint64_t number = decimal(argv[++index]);if (number > INT_MAX) return 2;pid = (pid_t)number; }
        else if (index + 1 < argc && !strcmp(argv[index], "--start")) ticks = decimal(argv[++index]);
        else { event("invalid_arguments", EINVAL);return 2; }
    }
    if (getuid() != 0 || !library || (strcmp(route, "path") && strcmp(route, "fd"))
        || !library_valid(library)) { event("preflight_rejected", EINVAL);return 2; }
    char name[256];
    if (!ticks || silph_process_name(pid, name, sizeof(name)) || strcmp(name, "com.nianticlabs.pokemongo")) {
        event("target_not_authorized", EINVAL);return 2;
    }
    if(managed==3?(appearance!=128u&&appearance!=640u):appearance!=0)return 2;
    int result = run(pid, ticks, library, route, seized, managed, appearance);
    return result;
}
