#define _GNU_SOURCE
#include "callback-memory.h"
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/uio.h>
#include <unistd.h>
_Static_assert(sizeof(SilphCallbackFrame)==800,"callback frame size");
_Static_assert(offsetof(SilphCallbackFrame,q)==288,"callback vector offset");
static bool readable(uintptr_t address){
    FILE *maps=fopen("/proc/self/maps","re");if(!maps)return false;char line[1024];bool found=false;
    for(unsigned i=0;i<16384&&fgets(line,sizeof(line),maps);i++){unsigned long long start=0,end=0;char mode[5]={0};if(sscanf(line,"%llx-%llx %4s",&start,&end,mode)==3&&mode[0]=='r'&&address>=start&&address<end&&sizeof(void *)<=end-address){found=true;break;}}
    fclose(maps);return found;
}
bool silph_callback_object(const void *object,const void *expected){
    uintptr_t address=(uintptr_t)object&UINT64_C(0x00ffffffffffffff);if(!expected||address<65536||address%sizeof(void *))return false;
    void *actual=NULL;struct iovec local={&actual,sizeof(actual)},remote={(void *)address,sizeof(actual)};
    ssize_t count=process_vm_readv(getpid(),&local,1,&remote,1,0);
    if(count!=(ssize_t)sizeof(actual)){if(count>=0||(errno!=ENOSYS&&errno!=EPERM&&errno!=EACCES)||!readable(address))return false;int descriptor=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);if(descriptor<0)return false;count=pread(descriptor,&actual,sizeof(actual),(off_t)address);close(descriptor);}
    return count==(ssize_t)sizeof(actual)&&actual==expected;
}
