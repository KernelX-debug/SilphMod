#define _GNU_SOURCE
#include "control.h"
#include "session-wire.h"
#include "kecleon-wire.h"
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static uint64_t number(const char *text) {
    if (!text || !*text || *text == '-' || *text == '+') return 0;
    char *end = NULL;errno = 0;uint64_t value = strtoull(text, &end, 10);
    return !errno && end && !*end ? value : 0;
}
static uint64_t start_ticks(uint32_t pid) {
    char filename[64],line[4096];snprintf(filename,sizeof(filename),"/proc/%u/stat",pid);
    FILE *file=fopen(filename,"re");if(!file)return 0;
    char *read=fgets(line,sizeof(line),file);fclose(file);if(!read)return 0;
    char *at=strrchr(line,')');if(!at)return 0;at+=2;
    for(unsigned field=3;field<22;field++){at=strchr(at,' ');if(!at)return 0;at++;}
    char *end=strchr(at,' ');if(end)*end=0;return number(at);
}
static uint64_t monotonic_ms(void){
    struct timespec value;if(clock_gettime(CLOCK_MONOTONIC,&value))return 0;
    return(uint64_t)value.tv_sec*1000u+(uint64_t)value.tv_nsec/1000000u;
}
static int credentials(uint64_t *high,uint64_t *low) {
    char text[43]={0};size_t used=0;
    uint64_t now=monotonic_ms();if(!now||now>UINT64_MAX-10000)return -1;
    uint64_t until=now+10000;
    while(used+1<sizeof(text)){
        now=monotonic_ms();if(!now||now>=until)return -1;
        struct pollfd input={.fd=STDIN_FILENO,.events=POLLIN};
        int ready=poll(&input,1,(int)(until-now));
        if(ready<0&&errno==EINTR)continue;
        if(ready<=0||!(input.revents&POLLIN))return -1;
        ssize_t count=read(STDIN_FILENO,text+used,1);
        if(count<0&&errno==EINTR)continue;
        if(count!=1)return -1;
        if(text[used]=='\n'){
            text[used]=0;char *separator=strchr(text,' ');
            if(!separator||strchr(separator+1,' '))return -1;
            *separator=0;*high=number(text);*low=number(separator+1);
            volatile char *clear=text;for(size_t i=0;i<sizeof(text);i++)clear[i]=0;
            return *high&&*low?0:-1;
        }
        if((text[used]<'0'||text[used]>'9')&&text[used]!=' ')return -1;
        used++;
    }
    return -1;
}
int main(int argc, char **argv) {
    if(argc!=3)return 2;
    uint64_t pid_value=number(argv[1]),ticks=number(argv[2]),key_hi=0,key_lo=0;
    if(pid_value<=1||pid_value>INT32_MAX||!ticks)return 2;
    if(credentials(&key_hi,&key_lo))return 2;
    uint32_t pid=(uint32_t)pid_value;
    if(start_ticks(pid)!=ticks)return 3;
    char name[SILPH_CONTROL_NAME_CAPACITY];struct sockaddr_un address;unsigned length;
    if(silph_control_name(name,sizeof(name),pid,ticks,key_hi)||silph_control_address(&address,name,&length))return 4;
    int descriptor=-1;
    for(unsigned attempt=0;attempt<900;attempt++) {
        if(start_ticks(pid)!=ticks)return 3;
        descriptor=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);if(descriptor<0)return 5;
        if(!connect(descriptor,(const struct sockaddr *)&address,length))break;
        int failure=errno;close(descriptor);descriptor=-1;
        if(failure!=ENOENT&&failure!=ECONNREFUSED&&failure!=EAGAIN){fprintf(stderr,"control connect errno=%d\n",failure);return 6;}
        struct timespec pause={0,50000000};nanosleep(&pause,NULL);
    }
    if(descriptor<0)return 6;
    struct ucred credentials={0};socklen_t bytes=sizeof(credentials);
    if(getsockopt(descriptor,SOL_SOCKET,SO_PEERCRED,&credentials,&bytes)||bytes!=sizeof(credentials)
        ||credentials.pid!=(pid_t)pid||start_ticks(pid)!=ticks){close(descriptor);return 7;}
    setvbuf(stdout,NULL,_IOLBF,0);
    printf("{\"event\":\"native_control_connected\",\"pid\":%u,\"serverUid\":%u}\n",pid,credentials.uid);
    uint64_t sequence=0;int result=0;char line[SILPH_CONTROL_BODY_CAPACITY*2+16];unsigned extended=0;
    while(fgets(line,sizeof(line),stdin)) {
        uint32_t command=0,foreground=0;uint8_t packet[sizeof(SilphControlRequest)+SILPH_CONTROL_BODY_CAPACITY]={0};size_t payload=0;
        if(!strcmp(line,"active\n")){command=SILPH_CONTROL_PULSE;foreground=1;}
        else if(!strcmp(line,"paused\n"))command=SILPH_CONTROL_PULSE;
        else if(!strcmp(line,"status\n"))command=SILPH_CONTROL_STATUS;
        else if(!strcmp(line,"stop\n"))command=SILPH_CONTROL_STOP;
        else if(!strncmp(line,"engine ",7)){
            size_t n=strlen(line+7);if(!n||line[7+n-1]!='\n'){result=8;break;}n--;
            if(!n||(n&1)||n>2*SILPH_CONTROL_BODY_CAPACITY){result=8;break;}
            for(size_t i=0;i<n/2;i++){
                unsigned value=0;for(unsigned j=0;j<2;j++){char ch=line[7+i*2+j];int v=ch>='0'&&ch<='9'?ch-'0':ch>='a'&&ch<='f'?ch-'a'+10:ch>='A'&&ch<='F'?ch-'A'+10:-1;
                    if(v<0){result=8;break;}value=(value<<4)|(unsigned)v;}if(result)break;packet[sizeof(SilphControlRequest)+i]=(uint8_t)value;
            }
            if(result)break;payload=n/2;command=SILPH_CONTROL_ENGINE;extended=1;
        }
        else {result=8;break;}
        if(sequence==UINT64_MAX||start_ticks(pid)!=ticks){result=9;break;}
        SilphControlRequest request={.magic=SILPH_CONTROL_MAGIC,.sequence=++sequence,.key_hi=key_hi,.key_lo=key_lo,
            .process_ticks=ticks,.abi=extended?SILPH_CONTROL_EXTENDED_ABI:SILPH_CONTROL_ABI,.size=(uint32_t)(sizeof(request)+payload),.pid=pid,.command=command,.foreground=foreground};
        memcpy(packet,&request,sizeof(request));
        if(send(descriptor,packet,request.size,MSG_NOSIGNAL)!=request.size){result=10;break;}
        struct pollfd poller={.fd=descriptor,.events=POLLIN};int ready;
        uint64_t started=monotonic_ms(),until=started+1500;
        do{
            uint64_t now=monotonic_ms();int remaining=now>=until?0:(int)(until-now);
            ready=poll(&poller,1,remaining);
        }while(ready<0&&errno==EINTR&&monotonic_ms()<until);
        int poll_error=ready<0?errno:0;
        SilphControlReply reply={0};uint8_t response[sizeof(reply)+SILPH_CONTROL_SNAPSHOT_CAPACITY]={0};ssize_t received=0;
        if(ready>0&&(poller.revents&POLLIN))received=recv(descriptor,response,sizeof(response),MSG_TRUNC);
        int receive_error=received<0?errno:0;
        if(received>=(ssize_t)sizeof(reply)&&received<=(ssize_t)sizeof(response))memcpy(&reply,response,sizeof(reply));
        if(ready<=0||!(poller.revents&POLLIN)||received<(ssize_t)sizeof(reply)||received>(ssize_t)sizeof(response)
            ||reply.magic!=SILPH_CONTROL_MAGIC||reply.abi!=request.abi||reply.size!=received
            ||reply.pid!=pid||reply.sequence!=sequence||start_ticks(pid)!=ticks){
            const char *failure=ready==0?"reply_timeout":ready<0?"poll_error":!(poller.revents&POLLIN)?"peer_closed"
                :received==0?"peer_closed":received<0?"receive_error":received<(ssize_t)sizeof(reply)||received>(ssize_t)sizeof(response)?"reply_size"
                :reply.magic!=SILPH_CONTROL_MAGIC||reply.abi!=request.abi||reply.size!=received?"reply_header"
                :reply.pid!=pid?"reply_pid":reply.sequence!=sequence?"reply_sequence":"process_changed";
            printf("{\"event\":\"native_control_failure\",\"stage\":\"%s\",\"sequence\":\"%" PRIu64 "\",\"command\":%u,\"waitMs\":%" PRIu64 ",\"pollError\":%d,\"receiveError\":%d,\"revents\":%d,\"received\":%zd}\n",
                failure,sequence,command,monotonic_ms()-started,poll_error,receive_error,poller.revents,received);
            result=11;break;
        }
        printf("{\"event\":\"native_control_reply\",\"sequence\":\"%" PRIu64 "\",\"command\":%u,\"status\":%u,"
            "\"state\":%u,\"reason\":%u,\"queued\":%u,\"readOnly\":%u,\"executed\":\"%" PRIu64 "\","
            "\"canceled\":\"%" PRIu64 "\",\"pulses\":\"%" PRIu64 "\"}\n",
            sequence,command,reply.status,reply.state,reply.reason,reply.queued,reply.readonly,reply.executed,reply.canceled,reply.pulses);
        unsigned finished=0;
        if(received>(ssize_t)sizeof(reply)){
            SilphSessionSnapshot snapshot;
            if((received!=sizeof(reply)+SILPH_SNAPSHOT_BYTES&&received!=sizeof(reply)+SILPH_KEC_BUNDLE_BYTES)||silph_snapshot_decode(response+sizeof(reply),SILPH_SNAPSHOT_BYTES,&snapshot)
                ||snapshot.pid!=pid||snapshot.ticks!=ticks||snapshot.session_hi!=key_hi){result=12;break;}
            printf("{\"event\":\"native_engine_snapshot\",\"bytes\":\"");
            for(unsigned i=sizeof(reply);i<sizeof(reply)+SILPH_SNAPSHOT_BYTES;i++)printf("%02x",response[i]);puts("\"}");
            if(received==sizeof(reply)+SILPH_KEC_BUNDLE_BYTES){
                SilphKecleonEvent event;
                if(silph_kec_event_decode(response+sizeof(reply)+SILPH_SNAPSHOT_BYTES,SILPH_KEC_EVENT_BYTES,&event)||event.identity.pid!=pid||event.identity.process_ticks!=ticks
                    ||event.identity.session_hi!=key_hi||event.identity.session_lo!=snapshot.session_lo||event.identity.account_hi!=snapshot.account_hi||event.identity.account_lo!=snapshot.account_lo){result=13;break;}
                printf("{\"event\":\"native_kecleon_event\",\"bytes\":\"");for(unsigned i=sizeof(reply)+SILPH_SNAPSHOT_BYTES;i<(unsigned)received;i++)printf("%02x",response[i]);puts("\"}");
            }
            finished=!!(snapshot.flags&8);
        }
        if(finished||(command==SILPH_CONTROL_STOP&&!extended))break;
    }
    close(descriptor);return result;
}
