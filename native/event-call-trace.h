#ifndef SILPH_EVENT_CALL_TRACE_H
#define SILPH_EVENT_CALL_TRACE_H
#define _GNU_SOURCE
#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static int event_trace_read(uintptr_t address,void *output,size_t bytes){
    if(address<65536||!bytes||bytes>64||address>UINTPTR_MAX-bytes)return 0;
    struct iovec local={output,bytes},remote={(void *)address,bytes};
    ssize_t count=process_vm_readv(getpid(),&local,1,&remote,1,0);
    if(count==(ssize_t)bytes)return 1;
    int fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);if(fd<0)return 0;
    count=pread(fd,output,bytes,(off_t)address);close(fd);return count==(ssize_t)bytes;
}
static void event_trace_pointer(unsigned slot,uintptr_t address){
    Dl_info image={0};unsigned char bytes[16]={0};char code[33]={0};
    int read=event_trace_read(address,bytes,sizeof(bytes));
    for(unsigned i=0;read&&i<sizeof(bytes);i++){static const char hex[]="0123456789abcdef";code[i*2]=hex[bytes[i]>>4];code[i*2+1]=hex[bytes[i]&15];}
    const char *name="unknown";uintptr_t relative=0;
    if(address>=65536&&dladdr((void *)address,&image)&&image.dli_fname&&address>=(uintptr_t)image.dli_fbase){
        const char *slash=strrchr(image.dli_fname,'/');name=slash?slash+1:image.dli_fname;relative=address-(uintptr_t)image.dli_fbase;
    }
    __android_log_print(ANDROID_LOG_INFO,"SilphEvent","call entry slot=%u image=%s rva=0x%llx readable=%d code=%s",slot,name,(unsigned long long)relative,read,code);
}
static void event_trace_call(const void *method,void *runtime){
    static atomic_bool recorded;
    if(atomic_exchange(&recorded,true))return;
    uintptr_t words[3]={0};int read=event_trace_read((uintptr_t)method,words,sizeof(words));
    __android_log_print(ANDROID_LOG_INFO,"SilphEvent","call contract method=bftu metadataRead=%d accountActionsUnchanged=true",read);
    for(unsigned i=0;read&&i<3;i++)event_trace_pointer(i,words[i]);event_trace_pointer(3,(uintptr_t)runtime);
}
#endif
