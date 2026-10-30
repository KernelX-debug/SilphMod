#define _GNU_SOURCE
#include "remote.h"
#include "elf-symbol.h"
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
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) {
    struct timespec sample;
    if (clock_gettime(CLOCK_MONOTONIC, &sample)) return 0;
    return (uint64_t)sample.tv_sec * 1000 + (uint64_t)sample.tv_nsec / 1000000;
}

static uint64_t process_stat(pid_t pid, char *state) {
    if (state) *state = 0;
    char path[64], buffer[4096];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *input = fopen(path, "re");
    if (!input) return 0;
    char *line = fgets(buffer, sizeof(buffer), input);
    fclose(input);
    if (!line) return 0;
    char *end = strrchr(line, ')');
    if (!end || end[1] != ' ') return 0;
    if (state) *state = end[2];
    char *save = NULL;
    char *token = strtok_r(end + 2, " ", &save);
    for (int field = 3; token; field++, token = strtok_r(NULL, " ", &save)) {
        if (field != 22) continue;
        char *tail = NULL;
        errno = 0;
        uint64_t ticks = strtoull(token, &tail, 10);
        return !errno && tail && !*tail ? ticks : 0;
    }
    return 0;
}

uint64_t silph_start_ticks(pid_t pid) { return process_stat(pid, NULL); }

int silph_process_check(pid_t pid, uint64_t ticks, char *state) {
    char observed = 0;
    uint64_t actual = process_stat(pid, &observed);
    if (state) *state = observed;
    if (pid <= 1 || !ticks || actual != ticks || !observed || observed == 'Z' || observed == 'X' || observed == 'x') {
        errno = ESRCH;return -1;
    }
    return 0;
}

int silph_process_name(pid_t pid, char *output, size_t capacity) {
    char path[64];
    if (pid <= 1 || capacity < 2) { errno = EINVAL; return -1; }
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t count = read(fd, output, capacity - 1);
    int error = errno;
    close(fd);
    if (count <= 0) { errno = count ? error : ENOENT; return -1; }
    output[count] = 0;
    if (!memchr(output, 0, (size_t)count)) { errno = EOVERFLOW; return -1; }
    return 0;
}

typedef struct {
    uint64_t start, end, offset, inode;
    unsigned major, minor;
    char permissions[5];
    char path[PATH_MAX];
} Mapping;

static int parse_map(const char *line, Mapping *map) {
    int consumed = 0;
    memset(map, 0, sizeof(*map));
    if (sscanf(line, "%" SCNx64 "-%" SCNx64 " %4s %" SCNx64 " %x:%x %" SCNu64 " %n",
               &map->start, &map->end, map->permissions, &map->offset, &map->major, &map->minor,
               &map->inode, &consumed) != 7 || map->start >= map->end || consumed <= 0) return -1;
    const char *path = line + consumed;
    size_t count = strcspn(path, "\r\n");
    if (count >= sizeof(map->path)) return -1;
    memcpy(map->path, path, count);
    return 0;
}

static FILE *maps_file(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    return fopen(path, "re");
}

static int memory_range(pid_t pid, uint64_t address, size_t size, int writable, const Mapping *identity) {
    if (!size || address > UINT64_MAX - size) return 0;
    FILE *input = maps_file(pid);
    if (!input) return 0;
    char *line = NULL;
    size_t capacity = 0;
    int found = 0;
    while (getline(&line, &capacity, input) >= 0) {
        Mapping map;
        if (parse_map(line, &map)) continue;
        if (address < map.start || address + size > map.end) continue;
        if (identity && (map.inode != identity->inode || map.major != identity->major || map.minor != identity->minor)) break;
        found = writable ? map.permissions[0] == 'r' && map.permissions[1] == 'w' : map.permissions[2] == 'x';
        break;
    }
    free(line);fclose(input);return found;
}

typedef struct { int fd; } FileSource;
typedef struct {
    pid_t pid;
    Mapping identity;
    int fd;
} MemorySource;

static void symbol_diagnostic(const char *module, const char *symbol, const char *method, const char *stage, int error) {
    fprintf(stdout, "{\"event\":\"native_symbol_resolution\",\"module\":\"%s\",\"symbol\":\"%s\",\"method\":\"%s\",\"stage\":\"%s\",\"errno\":%d}\n",
        module, symbol, method, stage, error);fflush(stdout);
}
static int file_source_read(void *context, uint64_t offset, void *output, size_t bytes) {
    FileSource *source = context;
    size_t copied = 0;
    while (copied < bytes) {
        ssize_t result = pread(source->fd, (char *)output + copied, bytes - copied, (off_t)(offset + copied));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) { if (!result) errno = ENOEXEC;return -1; }
        copied += (size_t)result;
    }
    return 0;
}
static int memory_source_read(void *context, uint64_t offset, void *output, size_t bytes) {
    MemorySource *source = context;
    if (offset > UINT64_MAX - source->identity.start || bytes > UINT64_MAX - source->identity.start - offset) { errno = EOVERFLOW;return -1; }
    uint64_t address = source->identity.start + offset, end = address + bytes, cursor = address;
    FILE *input = maps_file(source->pid);
    if (!input) return -1;
    char *line = NULL;
    size_t capacity = 0;
    while (cursor < end && getline(&line, &capacity, input) >= 0) {
        Mapping map;
        if (parse_map(line, &map) || cursor < map.start || cursor >= map.end) continue;
        if (map.permissions[0] != 'r' || map.inode != source->identity.inode
            || map.major != source->identity.major || map.minor != source->identity.minor) break;
        cursor = map.end < end ? map.end : end;
    }
    free(line);fclose(input);
    if (cursor != end) { errno = EFAULT;return -1; }
    size_t copied = 0;
    while (copied < bytes) {
        struct iovec local = {(char *)output + copied, bytes - copied};
        struct iovec remote = {(void *)(uintptr_t)(address + copied), bytes - copied};
        ssize_t result = process_vm_readv(source->pid, &local, 1, &remote, 1, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0 && source->fd >= 0) {
            result = pread(source->fd, (char *)output + copied, bytes - copied, (off_t)(address + copied));
            if (result < 0 && errno == EINTR) continue;
        }
        if (result <= 0) { if (!result) errno = EFAULT;return -1; }
        copied += (size_t)result;
    }
    return 0;
}
static int memory_source_executable(void *context, uint64_t virtual_address, uint64_t file_offset, uint64_t base) {
    MemorySource *source = context;
    if (virtual_address < base || virtual_address - base > UINT64_MAX - source->identity.start) { errno = ENOEXEC;return -1; }
    uint64_t address = source->identity.start + virtual_address - base;
    if (address > UINT64_MAX - 4) { errno = ENOEXEC;return -1; }
    FILE *input = maps_file(source->pid);
    if (!input) return -1;
    char *line = NULL;
    size_t capacity = 0;
    int found = 0;
    while (getline(&line, &capacity, input) >= 0) {
        Mapping map;
        if (parse_map(line, &map) || address < map.start || address + 4 > map.end) continue;
        if (map.permissions[2] == 'x' && map.inode == source->identity.inode && map.major == source->identity.major
            && map.minor == source->identity.minor && address - map.start <= UINT64_MAX - map.offset
            && map.offset + address - map.start == file_offset) found = 1;
        break;
    }
    free(line);fclose(input);return found;
}
static int symbol_address(pid_t pid, const Mapping *headers, size_t count, uint64_t value, uint64_t base, uint64_t *output) {
    uint64_t address = 0;
    for (size_t candidate = 0; candidate < count; candidate++) {
        if (headers[candidate].start < base) continue;
        uint64_t bias = headers[candidate].start - base;
        if (value > UINT64_MAX - bias) continue;
        uint64_t possible = bias + value;
        if (!memory_range(pid, possible, 4, 0, &headers[candidate])) continue;
        if (address && possible != address) { errno = EEXIST;return -1; }
        address = possible;
    }
    if (!address) { errno = EFAULT;return -1; }
    *output = address;return 0;
}
static int remote_symbol_once(pid_t pid, const char *module, const char *symbol, uint64_t *output) {
    if (!module || !symbol || !output || strspn(module, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != strlen(module)
        || strspn(symbol, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(symbol)) { errno = EINVAL;return -1; }
    uint64_t ticks = silph_start_ticks(pid);
    if (!ticks) { errno = ESRCH;return -1; }
    FILE *input = maps_file(pid);
    if (!input) return -1;
    Mapping first, headers[16];
    memset(&first, 0, sizeof(first));
    size_t header_count = 0;
    char *line = NULL;
    size_t capacity = 0;
    int ambiguous = 0;
    while (getline(&line, &capacity, input) >= 0) {
        Mapping map;
        if (parse_map(line, &map) || map.offset || !map.inode || map.path[0] != '/') continue;
        const char *base = strrchr(map.path, '/');
        if (!base || strcmp(base + 1, module)) continue;
        if (first.inode && (first.inode != map.inode || first.major != map.major || first.minor != map.minor)) { ambiguous = 1;break; }
        if (header_count == 16) { ambiguous = 1;break; }
        if (!first.inode) first = map;
        headers[header_count++] = map;
    }
    free(line);fclose(input);
    if (!first.inode || ambiguous) {
        errno = ambiguous ? EEXIST : ENOENT;symbol_diagnostic(module, symbol, "maps", "module_identity", errno);return -1;
    }
    const uint64_t page = (uint64_t)sysconf(_SC_PAGESIZE);
    int failure = ENOEXEC, definitive = 0;
    uint64_t value = 0, base = 0, address = 0;
    char path[PATH_MAX + 64];
    for (unsigned method = 0; method < 2; method++) {
        const char *name = method ? "map_files" : "target_root";
        if (method) snprintf(path, sizeof(path), "/proc/%d/map_files/%" PRIx64 "-%" PRIx64, pid, first.start, first.end);
        else snprintf(path, sizeof(path), "/proc/%d/root%s", pid, first.path);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) { failure = errno;symbol_diagnostic(module, symbol, name, "open", failure);continue; }
        struct stat status;
        const char *stage = "file_stat";
        int ready = !fstat(fd, &status);
        if (!ready) failure = errno;
        else if (status.st_size < (off_t)sizeof(Elf64_Ehdr) || status.st_size > 128 * 1024 * 1024) { ready = 0;failure = ENOEXEC;stage = "file_size"; }
        else if ((uint64_t)status.st_ino != first.inode || major(status.st_dev) != first.major || minor(status.st_dev) != first.minor) {
            ready = 0;failure = ESTALE;stage = "file_identity";
            fprintf(stdout, "{\"event\":\"native_symbol_file_identity\",\"module\":\"%s\",\"method\":\"%s\",\"mappedInode\":\"%" PRIu64
                "\",\"mappedDevice\":\"%x:%x\",\"fileInode\":\"%" PRIu64 "\",\"fileDevice\":\"%x:%x\"}\n",
                module, name, first.inode, first.major, first.minor, (uint64_t)status.st_ino, major(status.st_dev), minor(status.st_dev));fflush(stdout);
        }
        if (ready) {
            FileSource file = {fd};
            SilphElfSource source = {.read=file_source_read,.context=&file,.bytes=(uint64_t)status.st_size,.page_size=page};
            if (silph_elf_symbol(&source, symbol, &value, &base)) { failure = errno;stage = source.stage; }
            else if (symbol_address(pid, headers, header_count, value, base, &address)) { failure = errno;stage = "executable_mapping";definitive = failure == EEXIST; }
            else {
                close(fd);
                if (silph_start_ticks(pid) != ticks) { errno = ESRCH;return -1; }
                *output = address;symbol_diagnostic(module, symbol, name, "complete", 0);return 0;
            }
        }
        close(fd);symbol_diagnostic(module, symbol, name, stage, failure);
        if (definitive) { errno = failure;return -1; }
    }
    snprintf(path, sizeof(path), "/proc/%d/mem", pid);
    int memory_fd = open(path, O_RDONLY | O_CLOEXEC);
    uint64_t selected = 0;
    int memory_failure = 0;
    for (size_t candidate = 0; candidate < header_count; candidate++) {
        MemorySource memory = {pid, headers[candidate], memory_fd};
        SilphElfSource source = {.read=memory_source_read,.executable=memory_source_executable,.context=&memory,.bytes=UINT64_C(134217728),
            .mapping_start=headers[candidate].start,.page_size=page,.memory=1};
        if (silph_elf_symbol(&source, symbol, &value, &base)) {
            failure = errno;symbol_diagnostic(module, symbol, "mapped_memory", source.stage, failure);
            if (failure != ENOENT || strcmp(source.stage, "executable_image")) memory_failure = 1;
            continue;
        }
        if (symbol_address(pid, &headers[candidate], 1, value, base, &address)) {
            failure = errno;symbol_diagnostic(module, symbol, "mapped_memory", "executable_mapping", failure);
            if (failure != EFAULT) memory_failure = 1;
            continue;
        }
        if (selected && selected != address) { failure = EEXIST;memory_failure = 1;break; }
        selected = address;
    }
    if (memory_fd >= 0) close(memory_fd);
    if (silph_start_ticks(pid) != ticks) { errno = ESRCH;return -1; }
    if (!selected || memory_failure) { errno = failure;return -1; }
    *output = selected;symbol_diagnostic(module, symbol, "mapped_memory", "complete", 0);return 0;
}

int silph_remote_symbol(pid_t pid, const char *module, const char *symbol, uint64_t *output) {
    uint64_t ticks = silph_start_ticks(pid);
    if (silph_process_check(pid, ticks, NULL)) return -1;
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        int result = remote_symbol_once(pid, module, symbol, output), error = errno;
        if (silph_process_check(pid, ticks, NULL)) return -1;
        if (!result) return 0;
        if (attempt == 2 || (error != EFAULT && error != ESTALE && error != EAGAIN && error != EINTR)) { errno = error;return -1; }
        symbol_diagnostic(module, symbol, "maps_retry", "mapping_changed", error);
        const struct timespec delay = {0, 10000000};nanosleep(&delay, NULL);
    }
    errno = EFAULT;return -1;
}
int silph_remote_read(pid_t pid, uint64_t address, void *output, size_t bytes) {
    if (!output || !bytes || address > UINT64_MAX - bytes) { errno = EINVAL;return -1; }
    size_t copied = 0;
    while (copied < bytes) {
        struct iovec local = {(char *)output + copied, bytes - copied};
        struct iovec remote = {(void *)(uintptr_t)(address + copied), bytes - copied};
        ssize_t count = process_vm_readv(pid, &local, 1, &remote, 1, 0);
        if (count > 0) { copied += (size_t)count;continue; }
        uint64_t at = (address + copied) & ~(uint64_t)(sizeof(long) - 1);
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, (void *)(uintptr_t)at, NULL);
        if (word == -1 && errno) return -1;
        size_t offset = (size_t)(address + copied - at), take = sizeof(word) - offset;
        if (take > bytes - copied) take = bytes - copied;
        memcpy((char *)output + copied, (char *)&word + offset, take);copied += take;
    }
    return 0;
}

int silph_remote_write(pid_t pid, uint64_t address, const void *input, size_t bytes) {
    if (!input || !bytes || address > UINT64_MAX - bytes) { errno = EINVAL;return -1; }
    size_t copied = 0;
    while (copied < bytes) {
        struct iovec local = {(char *)input + copied, bytes - copied};
        struct iovec remote = {(void *)(uintptr_t)(address + copied), bytes - copied};
        ssize_t count = process_vm_writev(pid, &local, 1, &remote, 1, 0);
        if (count > 0) { copied += (size_t)count;continue; }
        uint64_t at = (address + copied) & ~(uint64_t)(sizeof(long) - 1);
        size_t offset = (size_t)(address + copied - at), take = sizeof(long) - offset;
        if (take > bytes - copied) take = bytes - copied;
        long word = 0;
        if (offset || take < sizeof(word)) {
            errno = 0;word = ptrace(PTRACE_PEEKDATA, pid, (void *)(uintptr_t)at, NULL);
            if (word == -1 && errno) return -1;
        }
        memcpy((char *)&word + offset, (const char *)input + copied, take);
        if (ptrace(PTRACE_POKEDATA, pid, (void *)(uintptr_t)at, (void *)(uintptr_t)word)) return -1;
        copied += take;
    }
    return 0;
}

static int wait_stop(SilphTrace *trace, int timeout) {
    uint64_t end = now_ms() + (unsigned)timeout;
    while (now_ms() < end) {
        int status = 0;
        pid_t result = waitpid(trace->pid, &status, __WALL | WNOHANG);
        if (result == trace->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) { trace->exited = 1;trace->attached = 0;errno = ESRCH;return -1; }
            if (WIFSTOPPED(status)) { trace->stopped = 1;return status; }
        }
        if (result < 0 && errno != EINTR) return -1;
        const struct timespec delay = {0, 2000000};nanosleep(&delay, NULL);
    }
    errno = ETIMEDOUT;return -1;
}

static int read_registers(pid_t pid, struct user_pt_regs *registers) {
    struct iovec data = {registers, sizeof(*registers)};
    if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &data) || data.iov_len != sizeof(*registers)) return -1;
    return 0;
}

static int capture_optional(pid_t pid, int note, unsigned char **output, size_t *bytes) {
    struct user_sve_header header;
    memset(&header, 0, sizeof(header));
    struct iovec data = {&header, sizeof(header)};
    if (ptrace(PTRACE_GETREGSET, pid, (void *)(intptr_t)note, &data)) {
        if (errno == EINVAL || errno == ENODEV || errno == EIO) return 0;
        return -1;
    }
    if (data.iov_len != sizeof(header) || header.size < sizeof(header) || header.size > 65536) { errno = ENOTSUP;return -1; }
    unsigned char *buffer = calloc(1, header.size);
    if (!buffer) return -1;
    data.iov_base = buffer;data.iov_len = header.size;
    if (ptrace(PTRACE_GETREGSET, pid, (void *)(intptr_t)note, &data) || data.iov_len != header.size) { free(buffer);return -1; }
    *output = buffer;*bytes = header.size;return 0;
}

static int quiescent_syscall(uint64_t number) {
    return number == __NR_epoll_pwait || number == __NR_ppoll || number == __NR_pselect6
        || number == __NR_nanosleep || number == __NR_clock_nanosleep
#ifdef __NR_epoll_pwait2
        || number == __NR_epoll_pwait2
#endif
        ;
}
static int read_syscall_number(pid_t pid,int *number){
    struct iovec data={number,sizeof(*number)};
    if(ptrace(PTRACE_GETREGSET,pid,(void *)NT_ARM_SYSTEM_CALL,&data))return -1;
    if(data.iov_len!=sizeof(*number)){errno=ENOTSUP;return -1;}return 0;
}
static int legacy_syscall_info(pid_t pid,const struct user_pt_regs *registers,struct ptrace_syscall_info *info){
    int number=-1;if(read_syscall_number(pid,&number))return -1;
    if((registers->pstate&PSR_MODE32_BIT)||registers->regs[7]>1||number<0||registers->regs[8]!=(uint64_t)number){errno=ENOTSUP;return -1;}
    memset(info,0,sizeof(*info));info->op=registers->regs[7]?PTRACE_SYSCALL_INFO_EXIT:PTRACE_SYSCALL_INFO_ENTRY;
    if(info->op==PTRACE_SYSCALL_INFO_ENTRY)info->entry.nr=(uint64_t)number;return 0;
}
#ifdef SILPH_LEGACY_HWSTEP
static int legacy_debug_quiet(pid_t pid){
    const int notes[]={NT_ARM_HW_BREAK,NT_ARM_HW_WATCH};
    for(unsigned i=0;i<2;i++){
        struct user_hwdebug_state debug={0};struct iovec data={&debug,sizeof(debug)};
        if(ptrace(PTRACE_GETREGSET,pid,(void *)(intptr_t)notes[i],&data))return -1;
        unsigned count=debug.dbg_info&0xffu;
        if(count>16||data.iov_len<8u+count*sizeof(debug.dbg_regs[0])||data.iov_len>sizeof(debug)){errno=ENOTSUP;return -1;}
        for(unsigned j=0;j<count;j++)if(debug.dbg_regs[j].ctrl&1u)return 0;
    }
    return 1;
}
static int legacy_user_boundary(SilphTrace *trace,struct user_pt_regs *registers){
    trace->stage="legacy_user_boundary";
    int quiet=legacy_debug_quiet(trace->pid);
    if(ptrace(PTRACE_SINGLESTEP,trace->pid,NULL,NULL))return -1;trace->stopped=0;
    int status=wait_stop(trace,1000);if(status<0)return -1;
    siginfo_t info={0};int info_ok=!ptrace(PTRACE_GETSIGINFO,trace->pid,NULL,&info);
    int read_ok=!read_registers(trace->pid,registers);
    int hardware_step=info_ok&&read_ok&&quiet==1&&info.si_code==TRAP_HWBKPT
        &&(uintptr_t)info.si_addr==registers->pc&&legacy_debug_quiet(trace->pid)==1;
    int recognized=info_ok&&WSTOPSIG(status)==SIGTRAP&&!((unsigned)status>>16)
        &&(info.si_code==TRAP_TRACE||hardware_step);
    fprintf(stdout,"{\"event\":\"native_boundary_step\",\"signal\":%d,\"eventCode\":%u,\"signalInfo\":%s,\"signalCode\":%d,\"debugQuiet\":%d,\"registersRead\":%s,\"accepted\":%s}\n",
        WSTOPSIG(status),(unsigned)status>>16,info_ok?"true":"false",info_ok?info.si_code:0,quiet,read_ok?"true":"false",recognized?"true":"false");fflush(stdout);
    if(!recognized){
        trace->delivery_signal=WSTOPSIG(status);errno=EINTR;return -1;
    }
    if(!read_ok)return -1;
    const uint64_t single_step_flag=UINT64_C(1)<<21;registers->pstate&=~single_step_flag;
    if((registers->pstate&PSR_MODE32_BIT)||!registers->pc||(registers->pc&3)||(registers->sp&15)){errno=ENOTSUP;return -1;}
    trace->delivery_signal=0;
    return 0;
}

#else
static int legacy_user_boundary(SilphTrace *trace,struct user_pt_regs *registers){
    trace->stage="legacy_user_boundary";
    if(ptrace(PTRACE_SINGLESTEP,trace->pid,NULL,NULL))return -1;trace->stopped=0;
    int status=wait_stop(trace,1000);if(status<0)return -1;
    siginfo_t info={0};
    if(WSTOPSIG(status)!=SIGTRAP||((unsigned)status>>16)||ptrace(PTRACE_GETSIGINFO,trace->pid,NULL,&info)||info.si_code!=TRAP_TRACE){
        trace->delivery_signal=WSTOPSIG(status);errno=EINTR;return -1;
    }
    if(read_registers(trace->pid,registers))return -1;
    const uint64_t single_step_flag=UINT64_C(1)<<21;registers->pstate&=~single_step_flag;
    if((registers->pstate&PSR_MODE32_BIT)||!registers->pc||(registers->pc&3)||(registers->sp&15)){errno=ENOTSUP;return -1;}
    return 0;
}

#endif

int silph_trace_open(SilphTrace *trace, pid_t pid, uint64_t ticks) {
    return silph_trace_open_mode(trace, pid, ticks, 1);
}
static char process_state(pid_t pid) {
    char state = 0;process_stat(pid, &state);return state;
}
int silph_trace_open_mode(SilphTrace *trace, pid_t pid, uint64_t ticks, int seized) {
    memset(trace, 0, sizeof(*trace));trace->pid = pid;trace->start_ticks = ticks;trace->stage = "identity";
    trace->process_fd = -1;
    trace->seized = seized;
    if (pid <= 1 || !ticks || silph_start_ticks(pid) != ticks || (seized != 0 && seized != 1)) { errno = ESRCH;goto fail; }
    char state = process_state(pid);
    if (!state || state == 'Z' || state == 'X') { errno = ESRCH;goto fail; }
    if (state == 'T' || state == 't') { trace->stage = "target_already_stopped";errno = EBUSY;goto fail; }
    if (!seized) {
        char path[64];snprintf(path, sizeof(path), "/proc/%d", pid);
        trace->process_fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (trace->process_fd < 0 || syscall(__NR_pidfd_send_signal, trace->process_fd, 0, NULL, 0)) {
            trace->stage = "legacy_resume_unavailable";errno = ENOTSUP;goto fail;
        }
        if (silph_start_ticks(pid) != ticks) { errno = ESRCH;goto fail; }
    }
    trace->stage = seized ? "ptrace_seize" : "ptrace_attach";
    if (ptrace(seized ? PTRACE_SEIZE : PTRACE_ATTACH, pid, NULL, seized ? (void *)(uintptr_t)PTRACE_O_TRACESYSGOOD : NULL)) goto fail;
    trace->attached = 1;
    trace->stage = seized ? "ptrace_interrupt" : "ptrace_attach_stop";
    if (seized && ptrace(PTRACE_INTERRUPT, pid, NULL, NULL)) goto fail;
    int status = wait_stop(trace, 1000);
    if (status < 0) goto fail;
    if (seized) {
        if ((unsigned)status >> 16 != PTRACE_EVENT_STOP) { trace->delivery_signal = WSTOPSIG(status);errno = EINTR;goto fail; }
    } else {
        for (unsigned attempt = 0; WSTOPSIG(status) != SIGSTOP; attempt++) {
            trace->delivery_signal = WSTOPSIG(status);
            if (attempt >= 16) { errno = EINTR;goto fail; }
            if (ptrace(PTRACE_CONT, pid, NULL, (void *)(intptr_t)trace->delivery_signal)) goto fail;
            trace->delivery_signal = 0;trace->stopped = 0;
            status = wait_stop(trace, 1000);
            if (status < 0) goto fail;
        }
        if (ptrace(PTRACE_SETOPTIONS, pid, NULL, (void *)(uintptr_t)PTRACE_O_TRACESYSGOOD)) goto fail;
    }
    trace->stage = "syscall_boundary";
    struct user_pt_regs initial;
    if (read_registers(pid, &initial)) goto fail;
    struct ptrace_syscall_info info={0};int legacy_info=0;
    if (ptrace(PTRACE_GET_SYSCALL_INFO, pid, sizeof(info), &info) < 0){
        int unsupported=errno;if(unsupported!=EIO&&unsupported!=EINVAL&&unsupported!=ENOSYS)goto fail;
        legacy_info=1;fprintf(stdout,"{\"event\":\"native_boundary_profile\",\"method\":\"arm64_regset_step\",\"syscallInfoErrno\":%d,\"pid\":%d}\n",unsupported,pid);fflush(stdout);
    }
    int current_syscall = -1;
    if (read_syscall_number(pid,&current_syscall)) goto fail;
    if(legacy_info)trace->stage="legacy_syscall_boundary";
    int awaiting = current_syscall >= 0 && quiescent_syscall((uint64_t)current_syscall) ? current_syscall : -1;
    uint64_t boundary_deadline = now_ms() + 1500;
    int idle_reported = 0;
    for (unsigned step = 0;; step++) {
        if (step >= 128 || now_ms() >= boundary_deadline) { errno = EBUSY;goto fail; }
        trace->stopped = 0;
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL)) goto fail;
        status = wait_stop(trace, 250);
        if (status < 0 && errno == ETIMEDOUT && awaiting >= 0 && now_ms() < boundary_deadline) {
            if (!idle_reported) {
                fprintf(stdout,"{\"event\":\"native_boundary_wait\",\"pid\":%d,\"startTicks\":\"%" PRIu64 "\",\"syscall\":%d}\n",pid,ticks,awaiting);fflush(stdout);idle_reported = 1;
            }
            uint64_t waiting_at = now_ms();
            if (waiting_at < boundary_deadline) status = wait_stop(trace, (int)(boundary_deadline - waiting_at));
            else errno = ETIMEDOUT;
        }
        if (status < 0) goto fail;
        if (WSTOPSIG(status) != (SIGTRAP | 0x80)) {
            siginfo_t information;
            memset(&information, 0, sizeof(information));
            int available = !ptrace(PTRACE_GETSIGINFO, pid, NULL, &information);
            fprintf(stdout, "{\"event\":\"native_boundary_signal\",\"signal\":%d,\"eventCode\":%u,\"signalInfo\":%s,\"signalCode\":%d,\"sender\":%d,\"step\":%u}\n",
                WSTOPSIG(status), (unsigned)status >> 16, available ? "true" : "false", available ? information.si_code : 0,
                available ? information.si_pid : 0, step);fflush(stdout);
            trace->delivery_signal = WSTOPSIG(status);errno = EINTR;goto fail;
        }
        if (read_registers(pid, &initial)) goto fail;
        memset(&info, 0, sizeof(info));
        if(legacy_info){if(legacy_syscall_info(pid,&initial,&info))goto fail;}
        else if (ptrace(PTRACE_GET_SYSCALL_INFO, pid, sizeof(info), &info) < 0) goto fail;
        if (info.op == PTRACE_SYSCALL_INFO_ENTRY) awaiting = quiescent_syscall(info.entry.nr) ? (int)info.entry.nr : -1;
        else if (info.op == PTRACE_SYSCALL_INFO_EXIT) {
            if (awaiting >= 0 && !((int64_t)initial.regs[0] <= -512 && (int64_t)initial.regs[0] >= -516)) {
                trace->quiescent_syscall = awaiting;break;
            }
            awaiting = -1;
        }
    }
    if(legacy_info){
        if(legacy_user_boundary(trace,&initial))goto fail;
        trace->legacy_boundary=1;
        fprintf(stdout,"{\"event\":\"native_boundary_ready\",\"method\":\"arm64_regset_step\",\"syscall\":%d,\"pid\":%d}\n",trace->quiescent_syscall,pid);fflush(stdout);
    }
    trace->stage = "save_registers";
    trace->original = initial;
    struct iovec fp = {&trace->fp, sizeof(trace->fp)};
    if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRFPREG, &fp) || fp.iov_len != sizeof(trace->fp)) goto fail;
    if (capture_optional(pid, NT_ARM_SVE, &trace->sve, &trace->sve_bytes)
        || capture_optional(pid, NT_ARM_SSVE, &trace->ssve, &trace->ssve_bytes)
        || capture_optional(pid, NT_ARM_ZA, &trace->za, &trace->za_bytes)) goto fail;
    trace->saved = 1;
    trace->stage = "save_borrowed_stack";
    if (initial.sp < sizeof(trace->stack_copy) || (initial.sp & 15)) { errno = EINVAL;goto fail; }
    trace->stack_copy_address = initial.sp - sizeof(trace->stack_copy);
    if (!memory_range(pid, trace->stack_copy_address, sizeof(trace->stack_copy), 1, NULL)) { errno = EFAULT;goto fail; }
    if (silph_remote_read(pid, trace->stack_copy_address, trace->stack_copy, sizeof(trace->stack_copy))) goto fail;
    trace->stack_saved = 1;
    if (silph_start_ticks(pid) != ticks) { errno = ESRCH;goto fail; }
    return 0;
fail:
    trace->error = errno;return -1;
}

int silph_trace_call(SilphTrace *trace, uint64_t function, const uint64_t *args,
                      size_t count, uint64_t *output, int borrowed_stack) {
    if (!trace->saved || !trace->stopped || trace->incomplete_call || !function || count > 8 || !output) { errno = EINVAL;return -1; }
    struct user_pt_regs registers = trace->original;
    for (size_t index = 0; index < 8; index++) registers.regs[index] = index < count ? args[index] : 0;
    registers.regs[30] = 0;
    registers.pc = function;
    registers.sp = borrowed_stack || !trace->stack ? (trace->original.sp - 256) & ~UINT64_C(15) : trace->stack;
    registers.pstate &= ~(uint64_t)PSR_BTYPE_MASK;
    int no_syscall = -1;
    struct iovec syscall = {&no_syscall, sizeof(no_syscall)};
    struct iovec data = {&registers, sizeof(registers)};
    if (ptrace(PTRACE_SETREGSET, trace->pid, (void *)NT_ARM_SYSTEM_CALL, &syscall)) return -1;
    trace->registers_dirty = 1;
    if (ptrace(PTRACE_SETREGSET, trace->pid, (void *)NT_PRSTATUS, &data)) return -1;
    trace->incomplete_call = 1;
    if (ptrace(PTRACE_CONT, trace->pid, NULL, NULL)) return -1;
    trace->stopped = 0;
    int status = wait_stop(trace, 2000);
    if (status < 0) { trace->error = errno;return -1; }
    siginfo_t information;
    memset(&information, 0, sizeof(information));
    int signal_read = (int)ptrace(PTRACE_GETSIGINFO, trace->pid, NULL, &information);
    int registers_read = read_registers(trace->pid, &registers);
    if (WSTOPSIG(status) != SIGSEGV || signal_read || registers_read || registers.pc != 0 || information.si_addr != NULL
        || information.si_code != SEGV_MAPERR) {
        trace->fault_signal = WSTOPSIG(status);trace->fault_code = signal_read ? 0 : information.si_code;
        trace->fault_pc = registers_read ? 0 : registers.pc;trace->fault_sp = registers_read ? 0 : registers.sp;
        trace->fault_address = signal_read ? 0 : (uint64_t)(uintptr_t)information.si_addr;
        trace->delivery_signal = WSTOPSIG(status);trace->error = EFAULT;errno = EFAULT;return -1;
    }
    trace->incomplete_call = 0;trace->controlled_returns++;
    *output = registers.regs[0];return 0;
}

static int restore_optional(pid_t pid, int note, unsigned char *buffer, size_t bytes) {
    if (!buffer) return 0;
    struct iovec data = {buffer, bytes};
    return ptrace(PTRACE_SETREGSET, pid, (void *)(intptr_t)note, &data) ? -1 : 0;
}

int silph_trace_close(SilphTrace *trace) {
    int failed = 0, external_stop = 0;
    if (trace->attached && !trace->stopped) {
        int interrupted = trace->seized ? (int)ptrace(PTRACE_INTERRUPT, trace->pid, NULL, NULL)
            : (int)syscall(__NR_tgkill, trace->pid, trace->pid, SIGSTOP);
        if (!interrupted && !trace->seized) trace->own_stop_requested = 1;
        int status = interrupted ? -1 : wait_stop(trace, 1000);
        if (status < 0) failed = 1;
        else if (WSTOPSIG(status) != (SIGTRAP | 0x80) && (unsigned)status >> 16 != PTRACE_EVENT_STOP) {
            siginfo_t info;
            memset(&info, 0, sizeof(info));
            int available = !ptrace(PTRACE_GETSIGINFO, trace->pid, NULL, &info);
            if (WSTOPSIG(status) == SIGSTOP && trace->own_stop_requested) {
                if (available && (info.si_code == SI_USER || info.si_code == SI_TKILL) && info.si_pid != getpid()) external_stop = 1;
            } else trace->delivery_signal = WSTOPSIG(status);
        }
    }
    if (trace->attached && trace->stopped && trace->registers_dirty && !trace->incomplete_call) {
        int no_syscall = -1;
        struct iovec syscall = {&no_syscall, sizeof(no_syscall)};
        struct iovec gpr = {&trace->original, sizeof(trace->original)};
        struct iovec fp = {&trace->fp, sizeof(trace->fp)};
        if (trace->stack_saved && silph_remote_write(trace->pid, trace->stack_copy_address, trace->stack_copy, sizeof(trace->stack_copy))) failed = 1;
        if (restore_optional(trace->pid, NT_ARM_ZA, trace->za, trace->za_bytes)) failed = 1;
        if (restore_optional(trace->pid, NT_ARM_SSVE, trace->ssve, trace->ssve_bytes)) failed = 1;
        if (restore_optional(trace->pid, NT_ARM_SVE, trace->sve, trace->sve_bytes)) failed = 1;
        if (ptrace(PTRACE_SETREGSET, trace->pid, (void *)NT_PRFPREG, &fp)) failed = 1;
        if (ptrace(PTRACE_SETREGSET, trace->pid, (void *)NT_ARM_SYSTEM_CALL, &syscall)) failed = 1;
        if (ptrace(PTRACE_SETREGSET, trace->pid, (void *)NT_PRSTATUS, &gpr)) failed = 1;
        struct user_pt_regs check;
        if (read_registers(trace->pid, &check) || memcmp(&check, &trace->original, sizeof(check))) failed = 1;
        trace->restored = !failed;
    } else if (!trace->registers_dirty) trace->restored = !failed;
    if (trace->incomplete_call) {
        failed = 1;
        trace->stage = "remote_call_incomplete_restart_required";
    }
    if (trace->attached && trace->stopped) {
        int signal = trace->incomplete_call || failed ? SIGSTOP : trace->delivery_signal;
        if (ptrace(PTRACE_DETACH, trace->pid, NULL, (void *)(intptr_t)signal)) failed = 1;
        else { trace->detached = 1;trace->attached = 0; }
    } else if (trace->exited || (!trace->attached && !trace->registers_dirty)) trace->detached = 1;
    if (trace->own_stop_requested && !external_stop && !failed && !trace->incomplete_call && trace->restored && trace->detached) {
        if (trace->process_fd < 0 || syscall(__NR_pidfd_send_signal, trace->process_fd, SIGCONT, NULL, 0)) {
            failed = 1;trace->restored = 0;trace->stage = "legacy_resume_failed";
        } else {
            fprintf(stdout, "{\"event\":\"native_owned_stop_resumed\",\"pid\":%d,\"pidfd\":true,\"registersRestored\":true}\n", trace->pid);fflush(stdout);
        }
    }
    if (trace->process_fd >= 0) { close(trace->process_fd);trace->process_fd = -1; }
    free(trace->sve);free(trace->ssve);free(trace->za);
    trace->sve = NULL;trace->ssve = NULL;trace->za = NULL;
    memset(trace->stack_copy, 0, sizeof(trace->stack_copy));
    return failed ? -1 : 0;
}
