#define _GNU_SOURCE
#include "code-memory.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

static int code_page(uintptr_t address,SilphCodePage *out){
    long size=sysconf(_SC_PAGESIZE);if(size!=4096&&size!=16384){errno=ENOTSUP;return -1;}
    uintptr_t page=address&~((uintptr_t)size-1);
    FILE *file=fopen("/proc/self/maps","re");if(!file)return -1;
    char line[4096],mode[5];int found=0;
    while(fgets(line,sizeof(line),file)){
        uintptr_t begin,end;uint64_t offset,inode;unsigned major,minor;
        if(sscanf(line,"%" SCNxPTR "-%" SCNxPTR " %4s %" SCNx64 " %x:%x %" SCNu64,&begin,&end,mode,&offset,&major,&minor,&inode)!=7||address<begin||address>=end)continue;
        if(mode[2]=='x'&&page>=begin&&(uintptr_t)size<=end-page&&offset<=UINT64_MAX-(page-begin)){
            *out=(SilphCodePage){.page=page,.offset=inode?offset+page-begin:0,.inode=inode,.major=major,.minor=minor,
                .protection=(mode[0]=='r'?PROT_READ:0)|(mode[1]=='w'?PROT_WRITE:0)|PROT_EXEC};found=1;
        }
        break;
    }
    fclose(file);if(!found)errno=EFAULT;return found?0:-1;
}
static int same_page(const SilphCodePage *expected){
    SilphCodePage current;
    return !code_page(expected->page,&current)&&current.page==expected->page&&current.offset==expected->offset
        &&current.inode==expected->inode&&current.major==expected->major&&current.minor==expected->minor;
}
static int valid_pages(const SilphCodeMemory *memory){
    if(!memory||memory->count<1||memory->count>2){errno=EINVAL;return -1;}
    for(unsigned i=0;i<memory->count;i++)if(!same_page(&memory->pages[i])){errno=ESTALE;return -1;}
    return 0;
}
static int permissions(SilphCodeMemory *memory,int readable){
    if(!memory||memory->count<1||memory->count>2){errno=EINVAL;return -1;}
    int error=0;
    if(readable&&valid_pages(memory)){memory->protect_errno=errno;return -1;}
    for(unsigned i=0;i<memory->count;i++){
        SilphCodePage *page=&memory->pages[i],before,after;
        int expected=page->protection|(readable?PROT_READ:0);
        if(!same_page(page)||code_page(page->page,&before)){errno=ESTALE;goto page_failed;}
        if(before.protection!=expected){
            memory->dirty_mask|=1u<<i;
            if(mprotect((void *)page->page,(size_t)sysconf(_SC_PAGESIZE),expected))goto page_failed;
        }
        if(!same_page(page)||code_page(page->page,&after)){errno=ESTALE;goto page_failed;}
        if(after.protection!=expected){errno=EACCES;goto page_failed;}
        if(expected==page->protection)memory->dirty_mask&=~(1u<<i);
        else memory->dirty_mask|=1u<<i;
        continue;
page_failed:
        if(!error)error=errno;
        if(readable)break;
    }
    if(error){if(readable)memory->protect_errno=error;else memory->restore_errno=error;errno=error;return -1;}
    return 0;
}
int silph_code_readable(SilphCodeMemory *memory){return permissions(memory,1);}
int silph_code_restore(SilphCodeMemory *memory){return permissions(memory,0);}
static int read_code(SilphCodeMemory *memory,unsigned char *output){
    if(valid_pages(memory))return -1;
    uintptr_t address=memory->address;size_t bytes=sizeof(memory->bytes);
    if(!address||address>UINTPTR_MAX-bytes){errno=EINVAL;return -1;}
    memory->read_method=0;
    SilphCodePage first,last;
    if(!code_page(address,&first)&&!code_page(address+bytes-1,&last)
        &&(first.protection&PROT_READ)&&(last.protection&PROT_READ)){
        memcpy(output,(const void *)address,bytes);memory->read_method=1;return 0;
    }
    struct iovec local={output,bytes},remote={(void *)address,bytes};
    ssize_t copied;
    do{copied=process_vm_readv(getpid(),&local,1,&remote,1,0);}while(copied<0&&errno==EINTR);
    if(copied==(ssize_t)bytes){memory->read_method=2;return 0;}
    memory->read_errno=copied<0?errno:EIO;
    int fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);
    size_t total=0;int error=0;
    if(fd<0)error=errno;
    while(fd>=0&&total<bytes){
        ssize_t count=pread(fd,output+total,bytes-total,(off_t)(address+total));
        if(count<0&&errno==EINTR)continue;
        if(count<=0){error=count?errno:EIO;break;}total+=(size_t)count;
    }
    if(fd>=0)close(fd);
    if(!error){memory->read_method=3;return 0;}
    memory->mem_errno=error;
    int failed=0;
    if(silph_code_readable(memory))failed=errno;
    else if(valid_pages(memory)||code_page(address,&first)||code_page(address+bytes-1,&last)
        ||!(first.protection&PROT_READ)||!(last.protection&PROT_READ))failed=ESTALE;
    else{memcpy(output,(const void *)address,bytes);memory->read_method=4;}
    if(silph_code_restore(memory)){errno=EUCLEAN;return -1;}
    if(failed){errno=failed;return -1;}return 0;
}
int silph_code_snapshot(const void *pointer,SilphCodeMemory *out){
    if(!out||!pointer||((uintptr_t)pointer&3)||(uintptr_t)pointer>UINTPTR_MAX-32){errno=EINVAL;return -1;}
    memset(out,0,sizeof(*out));out->address=(uintptr_t)pointer;
    if(code_page(out->address,&out->pages[0]))return -1;out->count=1;
    if(out->address+31>=out->pages[0].page+(uintptr_t)sysconf(_SC_PAGESIZE)){
        if(code_page(out->address+31,&out->pages[1]))return -1;out->count=2;
    }
    if(read_code(out,out->bytes))return -1;
    for(unsigned i=0;i<out->count;i++)if(!same_page(&out->pages[i])){errno=ESTALE;return -1;}
    return 0;
}
int silph_code_matches(SilphCodeMemory *memory){
    if(valid_pages(memory))return -1;
    unsigned char actual[16];if(read_code(memory,actual))return -1;
    if(memcmp(actual,memory->bytes,sizeof(actual))){errno=ESTALE;return -1;}return 0;
}
