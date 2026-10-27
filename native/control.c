#define _GNU_SOURCE
#include "control.h"
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define CONTEXT_MAGIC UINT32_C(0x53434331)
typedef struct {
    uint32_t magic, owner_tid;
    int listener, peer;
    uint64_t peer_deadline;
    SilphControlConfig config;
    SilphControlMetrics metrics;
} Control;
_Static_assert(sizeof(SilphControlRequest) == 64, "Control request ABI");
_Static_assert(sizeof(SilphControlReply) == 72, "Control reply ABI");

size_t silph_control_size(void) { return sizeof(Control); }
int silph_control_name(char *out, size_t capacity, uint32_t pid, uint64_t ticks, uint64_t key) {
    if (!out || capacity < SILPH_CONTROL_NAME_CAPACITY || !pid || !ticks || !key) return SILPH_CONTROL_ARGUMENT;
    int size = snprintf(out, capacity, "silph.control.%u.%" PRIu64 ".%016" PRIx64, pid, ticks, key);
    return size > 0 && (size_t)size < capacity ? SILPH_CONTROL_OK : SILPH_CONTROL_ARGUMENT;
}
int silph_control_address(void *memory, const char *name, unsigned *length) {
    if (!memory || !name || !length) return SILPH_CONTROL_ARGUMENT;
    size_t size = strnlen(name, SILPH_CONTROL_NAME_CAPACITY);
    if (!size || size >= SILPH_CONTROL_NAME_CAPACITY) return SILPH_CONTROL_ARGUMENT;
    struct sockaddr_un *address = memory;
    memset(address, 0, sizeof(*address));address->sun_family = AF_UNIX;
    memcpy(address->sun_path + 1, name, size);
    *length = (unsigned)(offsetof(struct sockaddr_un, sun_path) + 1 + size);
    return SILPH_CONTROL_OK;
}
static bool valid(Control *state) { return state && state->magic == CONTEXT_MAGIC; }
static bool owned(Control *state) { return state->owner_tid == (uint32_t)syscall(__NR_gettid); }
static uint64_t monotonic_ns(void) {
    struct timespec value;
    if(clock_gettime(CLOCK_MONOTONIC,&value))return 0;
    return (uint64_t)value.tv_sec*UINT64_C(1000000000)+(uint64_t)value.tv_nsec;
}
static void drop(Control *state) {
    if (state->peer < 0) return;
    close(state->peer);state->peer = -1;state->peer_deadline=0;state->metrics.connected = 0;
    if (state->metrics.authenticated) {
        state->metrics.disconnects++;
        silph_schedule_stop(state->config.scheduler, state->config.lease);
    }
}
int silph_control_open(void *memory, size_t bytes, const SilphControlConfig *config) {
    if (!memory || bytes != sizeof(Control) || (uintptr_t)memory % _Alignof(Control) || !config || !config->scheduler
        || !config->lease.epoch || !config->key_hi || !config->key_lo || config->reserved || !config->identity.pid
        || !config->identity.process_ticks || config->identity.reserved || (!!config->message != !!config->snapshot)
        || (config->message&&!config->extension)) return SILPH_CONTROL_ARGUMENT;
    Control *state = memory;
    if (state->magic == CONTEXT_MAGIC) return SILPH_CONTROL_ARGUMENT;
    char name[SILPH_CONTROL_NAME_CAPACITY];struct sockaddr_un address;unsigned length;
    if (silph_control_name(name, sizeof(name), config->identity.pid, config->identity.process_ticks, config->key_hi)
        || silph_control_address(&address, name, &length)) return SILPH_CONTROL_ARGUMENT;
    int listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listener < 0) return SILPH_CONTROL_IO;
    if (bind(listener, (const struct sockaddr *)&address, length) || listen(listener, 1)) {
        close(listener);return SILPH_CONTROL_IO;
    }
    memset(state, 0, bytes);state->config = *config;state->listener = listener;state->peer = -1;state->magic = CONTEXT_MAGIC;
    state->owner_tid = (uint32_t)syscall(__NR_gettid);
    return SILPH_CONTROL_OK;
}
static int accept_peer(Control *state) {
    int peer = accept4(state->listener, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (peer < 0) return errno == EAGAIN || errno == EINTR ? SILPH_CONTROL_OK : SILPH_CONTROL_IO;
    struct ucred credentials = {0};socklen_t bytes = sizeof(credentials);
    if (state->peer >= 0 || state->metrics.authenticated
        || getsockopt(peer, SOL_SOCKET, SO_PEERCRED, &credentials, &bytes) || bytes != sizeof(credentials)
        || credentials.pid <= 0 || credentials.uid != state->config.controller_uid) {
        close(peer);state->metrics.rejected++;return SILPH_CONTROL_OK;
    }
    uint64_t now=monotonic_ns();
    if(!now||now>UINT64_MAX-UINT64_C(5000000000)){close(peer);state->metrics.rejected++;return SILPH_CONTROL_OK;}
    state->peer = peer;state->peer_deadline=now+UINT64_C(5000000000);state->metrics.connected = 1;return SILPH_CONTROL_OK;
}
static void expire_peer(Control *state) {
    if(state->peer<0||state->metrics.authenticated)return;
    uint64_t now=monotonic_ns();
    if(!now||now>=state->peer_deadline){state->metrics.rejected++;drop(state);}
}
static bool request_valid(Control *state, const SilphControlRequest *request, ssize_t bytes) {
    bool extended=request->abi==SILPH_CONTROL_EXTENDED_ABI&&state->config.message;
    bool payload=extended&&request->command==SILPH_CONTROL_ENGINE&&bytes>(ssize_t)sizeof(*request)&&bytes<=(ssize_t)(sizeof(*request)+SILPH_CONTROL_BODY_CAPACITY);
    return (payload||bytes==sizeof(*request)) && request->magic == SILPH_CONTROL_MAGIC && (extended||request->abi==SILPH_CONTROL_ABI)
        && request->size == bytes && !request->reserved && request->pid == state->config.identity.pid
        && request->process_ticks == state->config.identity.process_ticks && request->key_hi == state->config.key_hi
        && request->key_lo == state->config.key_lo && state->metrics.sequence != UINT64_MAX
        && request->sequence == state->metrics.sequence + 1 && request->foreground <= 1
        && request->command >= SILPH_CONTROL_PULSE && request->command <= SILPH_CONTROL_ENGINE
        && (request->command!=SILPH_CONTROL_ENGINE||payload)
        && (request->command == SILPH_CONTROL_PULSE || !request->foreground);
}
static int receive(Control *state) {
    uint8_t packet[sizeof(SilphControlRequest)+SILPH_CONTROL_BODY_CAPACITY]={0};SilphControlRequest request = {0};
    ssize_t bytes = recv(state->peer,packet,sizeof(packet),MSG_DONTWAIT|MSG_TRUNC);
    if (bytes < 0 && (errno == EAGAIN || errno == EINTR)) return SILPH_CONTROL_OK;
    if (bytes <= 0) { drop(state);return SILPH_CONTROL_OK; }
    if(bytes>=(ssize_t)sizeof(request)&&bytes<=(ssize_t)sizeof(packet))memcpy(&request,packet,sizeof(request));
    if (!request_valid(state, &request, bytes)) { state->metrics.rejected++;drop(state);return SILPH_CONTROL_OK; }
    state->metrics.authenticated = 1;state->peer_deadline=0;state->metrics.sequence = request.sequence;state->metrics.accepted++;
    int status = SILPH_SCHEDULE_OK;
    if (request.command == SILPH_CONTROL_PULSE) {
        status = silph_schedule_pulse(state->config.scheduler, state->config.lease, &state->config.identity, request.foreground != 0);
        if (!status) state->metrics.pulses++;
    } else if (request.command == SILPH_CONTROL_STOP) status = silph_schedule_stop(state->config.scheduler, state->config.lease);
    else if(request.command==SILPH_CONTROL_ENGINE)status=state->config.message(state->config.extension,packet+sizeof(request),(size_t)bytes-sizeof(request));
    bool committed=request.command==SILPH_CONTROL_ENGINE&&!status;
    SilphScheduleMetrics metrics = {0};
    int snapshot = silph_schedule_metrics(state->config.scheduler, state->config.lease, &metrics);
    if (!status && snapshot&&!committed) status = snapshot;
    SilphControlReply reply = {.magic=SILPH_CONTROL_MAGIC,.sequence=request.sequence,.executed=metrics.executed,
        .canceled=metrics.canceled,.pulses=state->metrics.pulses,.abi=request.abi,.size=sizeof(reply),
        .pid=state->config.identity.pid,.status=(uint32_t)status,.state=metrics.state,.reason=metrics.reason,
        .queued=metrics.queued,.readonly=metrics.readonly};
    uint8_t response[sizeof(reply)+SILPH_CONTROL_SNAPSHOT_CAPACITY]={0};size_t extra=0;
    if(request.abi==SILPH_CONTROL_EXTENDED_ABI){
        int sampled=state->config.snapshot(state->config.extension,response+sizeof(reply),SILPH_CONTROL_SNAPSHOT_CAPACITY,&extra);
        if(sampled||extra>SILPH_CONTROL_SNAPSHOT_CAPACITY){extra=0;if(!reply.status&&!committed)reply.status=sampled?sampled:SILPH_CONTROL_ARGUMENT;}
    }
    reply.size=(uint32_t)(sizeof(reply)+extra);memcpy(response,&reply,sizeof(reply));
    if (send(state->peer,response,reply.size,MSG_DONTWAIT|MSG_NOSIGNAL)!=reply.size)drop(state);
    return SILPH_CONTROL_OK;
}
int silph_control_poll(void *memory, unsigned timeout_ms) {
    Control *state = memory;
    if (!valid(state) || timeout_ms > 250) return SILPH_CONTROL_ARGUMENT;
    if (!owned(state)) return SILPH_CONTROL_THREAD;
    if (state->metrics.closed) return SILPH_CONTROL_CLOSED;
    expire_peer(state);
    struct pollfd descriptors[2] = {{.fd=state->listener,.events=POLLIN},{.fd=state->peer,.events=POLLIN}};
    int count = poll(descriptors, 2, (int)timeout_ms);
    if (count < 0) return errno == EINTR ? SILPH_CONTROL_OK : SILPH_CONTROL_IO;
    expire_peer(state);
    if (descriptors[0].revents & POLLIN) {
        int result = accept_peer(state);if (result) return result;
    }
    if (state->peer>=0 && descriptors[1].fd==state->peer && descriptors[1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) return receive(state);
    return SILPH_CONTROL_OK;
}
int silph_control_metrics(void *memory, SilphControlMetrics *output) {
    if (!output) return SILPH_CONTROL_ARGUMENT;
    memset(output, 0, sizeof(*output));Control *state = memory;
    if (!valid(state)) return SILPH_CONTROL_ARGUMENT;
    if (!owned(state)) return SILPH_CONTROL_THREAD;
    *output = state->metrics;return SILPH_CONTROL_OK;
}
int silph_control_close(void *memory) {
    Control *state = memory;
    if (!valid(state)) return SILPH_CONTROL_ARGUMENT;
    if (!owned(state)) return SILPH_CONTROL_THREAD;
    if (!state->metrics.closed) {
        drop(state);close(state->listener);state->listener = -1;state->metrics.closed = 1;
    }
    return SILPH_CONTROL_OK;
}
