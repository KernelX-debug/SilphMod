#define _GNU_SOURCE
#include "startup-thread.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int (*entry)(SilphNativeReport *,uint32_t);
    SilphNativeReport report;
    int result;
} StartupJob;

static int reject(SilphNativeReport *report,int code,int error,int unfinished) {
    memset(report,0,sizeof(*report));
    report->magic=SILPH_NATIVE_MAGIC;report->abi=SILPH_NATIVE_ABI;report->bytes=sizeof(*report);
    report->pid=(uint32_t)getpid();report->page_size=(uint32_t)sysconf(_SC_PAGESIZE);
    report->engine_capabilities=SILPH_NATIVE_CAPABILITIES;report->managed_abi=1;
    report->managed_closed=!unfinished;report->managed_threads_restored=!unfinished;
    snprintf(report->detail,sizeof(report->detail),"Native startup thread: code=%d errno=%d pending=%d",code,error,unfinished);
    return code;
}

static void *start(void *value) {
    StartupJob *job=value;
    pthread_attr_t attributes;void *base=NULL;size_t bytes=0;
    int error=pthread_getattr_np(pthread_self(),&attributes);
    if(error){job->result=reject(&job->report,91,error,0);return NULL;}
    error=pthread_attr_getstack(&attributes,&base,&bytes);pthread_attr_destroy(&attributes);
    uintptr_t marker=(uintptr_t)&attributes,low=(uintptr_t)base;
    if(error||!low||bytes<65536||low>UINTPTR_MAX-bytes||marker<low||marker-low<16384||marker>=low+bytes){
        job->result=reject(&job->report,91,error?error:EFAULT,0);return NULL;
    }
    job->result=job->entry(&job->report,sizeof(job->report));return NULL;
}

int silph_startup_thread(int (*entry)(SilphNativeReport *,uint32_t),SilphNativeReport *report,uint32_t capacity) {
    if(!entry||!report||capacity<sizeof(*report))return 1;
    StartupJob *job=calloc(1,sizeof(*job));if(!job)return reject(report,90,ENOMEM,0);
    job->entry=entry;
    pthread_t thread;int error=pthread_create(&thread,NULL,start,job);
    if(error){free(job);return reject(report,90,error,0);}
    error=pthread_join(thread,NULL);
    if(error)return reject(report,92,error,1);
    int result=job->result;memcpy(report,&job->report,sizeof(*report));free(job);return result;
}
