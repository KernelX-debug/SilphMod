#ifndef SILPH_REMOTE_H
#define SILPH_REMOTE_H

#include <asm/ptrace.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    uint64_t start_ticks;
    int attached;
    int seized;
    int stopped;
    int exited;
    int saved;
    int registers_dirty;
    int incomplete_call;
    int delivery_signal;
    int controlled_returns;
    int quiescent_syscall;
    int legacy_boundary;
    int restored;
    int detached;
    int process_fd;
    int own_stop_requested;
    uint64_t stack;
    struct user_pt_regs original;
    struct user_fpsimd_state fp;
    unsigned char *sve;
    size_t sve_bytes;
    unsigned char *za;
    size_t za_bytes;
    unsigned char *ssve;
    size_t ssve_bytes;
    unsigned char stack_copy[16384];
    uint64_t stack_copy_address;
    int stack_saved;
    const char *stage;
    int error;
    int fault_signal, fault_code;
    uint64_t fault_pc, fault_address, fault_sp;
} SilphTrace;

uint64_t silph_start_ticks(pid_t);
int silph_process_check(pid_t, uint64_t, char *);
int silph_process_name(pid_t, char *, size_t);
int silph_remote_symbol(pid_t, const char *, const char *, uint64_t *);
int silph_remote_read(pid_t, uint64_t, void *, size_t);
int silph_remote_write(pid_t, uint64_t, const void *, size_t);
int silph_trace_open(SilphTrace *, pid_t, uint64_t);
int silph_trace_open_mode(SilphTrace *, pid_t, uint64_t, int);
int silph_trace_call(SilphTrace *, uint64_t, const uint64_t *, size_t, uint64_t *, int);
int silph_trace_close(SilphTrace *);

#endif
