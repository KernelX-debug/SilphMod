#define _GNU_SOURCE
#define silph_native_managed_probe silph_native_baseline_managed_probe
#include "standalone/observer.c"
#undef silph_native_managed_probe
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

typedef struct {
    void *klass;
    const void *method;
    uintptr_t address;
    uint32_t pointer_offset;
    SilphManagedMetrics metrics;
} MapContract;
typedef struct {
    const void *(*field)(void *,const char *);
    uint32_t (*field_flags)(const void *);
    const void *(*field_type)(const void *);
    void (*static_get)(const void *,void *);
    void (*field_get)(void *,const void *,void *);
    void (*class_init)(void *);
} Fields;
static int read_process(uintptr_t address,void *output,size_t bytes) {
    if(!address||!output||!bytes||address>UINTPTR_MAX-bytes){errno=EINVAL;return -1;}
    FILE *maps=fopen("/proc/self/maps","re");if(!maps)return -1;
    char line[1024];int readable=0;
    for(unsigned i=0;i<32768&&fgets(line,sizeof(line),maps);i++) {
        uintptr_t begin=0,end=0;char permissions[5]={0};
        if(sscanf(line,"%" SCNxPTR "-%" SCNxPTR " %4s",&begin,&end,permissions)==3
            &&permissions[0]=='r'&&address>=begin&&address+bytes<=end){readable=1;break;}
    }
    fclose(maps);
    if(!readable){errno=EFAULT;return -1;}
    memcpy(output,(const void *)address,bytes);return 0;
}
static int field_kind(const Fields *fields,const Metadata *meta,const void *field,uint32_t flags,int type) {
    return field&&((fields->field_flags(field)&0x10)==flags)&&meta->type_kind(fields->field_type(field))==type;
}
static int delegate_offset(const Fields *fields,const Metadata *meta,const SilphManagedApi *api,const Image *image,
    void *state,void *module,const char *name,uint32_t *offset) {
    const void *field=fields->field(module,name);
    if(!field_kind(fields,meta,field,0x10,0x12))return 31;
    void *delegate=NULL;fields->static_get(field,&delegate);
    if(!delegate)return 32;
    SilphManagedRef ref={0};int status=silph_managed_retain(state,delegate,false,&ref);
    if(status)return status;
    void *held=NULL;status=silph_managed_target(state,ref,&held);
    if(status)return status;
    void *klass=api->object_get_class(held);
    const void *ptr=fields->field(klass,"method_ptr"),*method=fields->field(klass,"method");
    if(!ptr||!method)return 34;
    if(!field_kind(fields,meta,ptr,0,0x18))return 256+meta->type_kind(fields->field_type(ptr));
    if(!field_kind(fields,meta,method,0,0x18))return 512+meta->type_kind(fields->field_type(method));
    uintptr_t target=0,metadata=0;fields->field_get(held,ptr,&target);fields->field_get(held,method,&metadata);
    uintptr_t values[16];
    if(!accessible(image,target,4,PF_X)||!metadata)return 37;
    if(read_process(metadata,values,sizeof(values)))return 1024+errno;
    for(uint32_t i=0;i<16;i++)if(values[i]==target){*offset=i*sizeof(uintptr_t);return 0;}
    return 39;
}
static int map_contract(const Image *image,MapContract *output) {
    memset(output,0,sizeof(*output));
    Metadata meta={0};Fields fields={0};SilphManagedApi api;
    int status=silph_managed_api_load(&api,resolve_managed,(void *)image);if(status)return status;
#define LOAD(member,name) do { *(void **)(&member)=export_address(image,name);if(!member)return 1; }while(0)
    LOAD(meta.assemblies,"il2cpp_domain_get_assemblies");LOAD(meta.image,"il2cpp_assembly_get_image");
    LOAD(meta.image_name,"il2cpp_image_get_name");LOAD(meta.klass,"il2cpp_class_from_name");LOAD(meta.method,"il2cpp_class_get_method_from_name");
    LOAD(meta.parameter_count,"il2cpp_method_get_param_count");LOAD(meta.return_type,"il2cpp_method_get_return_type");LOAD(meta.type_kind,"il2cpp_type_get_type");
    LOAD(fields.field,"il2cpp_class_get_field_from_name");LOAD(fields.field_flags,"il2cpp_field_get_flags");LOAD(fields.field_type,"il2cpp_field_get_type");
    LOAD(fields.static_get,"il2cpp_field_static_get_value");LOAD(fields.field_get,"il2cpp_field_get_value");LOAD(fields.class_init,"il2cpp_runtime_class_init");
#undef LOAD
    size_t bytes=silph_managed_size(4);void *state=calloc(1,bytes);if(!state)return 1;
    status=silph_managed_open(state,bytes,&api,4,64);
    if(status){free(state);return status;}
    size_t count=0;const void **assemblies=meta.assemblies(api.domain_get(),&count);
    if(!assemblies||!count||count>2048){status=1;goto cleanup;}
    const void *corlib=find_image(&meta,assemblies,count,"mscorlib.dll"),*game=find_image(&meta,assemblies,count,"holo-game.dll");
    void *module=corlib?meta.klass(corlib,"System.Reflection","Module"):NULL;
    output->klass=game?meta.klass(game,"Niantic.Holoholo.Map","MapExploreState"):NULL;
    output->method=output->klass?meta.method(output->klass,"get_IsActiveAndVisible",0):NULL;
    uint32_t flags=0;
    if(!module||!output->method||meta.parameter_count(output->method)||meta.type_kind(meta.return_type(output->method))!=2
        ||(api.method_get_flags(output->method,&flags)&0x10)||api.method_is_generic(output->method)||api.method_is_inflated(output->method)){
        status=1;goto cleanup;
    }
    fields.class_init(module);
    uint32_t first=UINT32_MAX,second=UINT32_MAX;
    status=delegate_offset(&fields,&meta,&api,image,state,module,"FilterTypeName",&first);
    if(!status)status=delegate_offset(&fields,&meta,&api,image,state,module,"FilterTypeNameIgnoreCase",&second);
    if(status||first!=second||first>120){if(!status)status=51;goto cleanup;}
    output->pointer_offset=first;
    if(read_process((uintptr_t)output->method+first,&output->address,sizeof(output->address))
        ||!accessible(image,output->address,16,PF_X)||(output->address&3)){status=1;goto cleanup;}
cleanup:;
    int closed=silph_managed_close(state,&output->metrics);
    if(!closed)free(state);
    return status?status:closed;
}
#ifndef SILPH_MAP_ENTRY
#define SILPH_MAP_ENTRY silph_native_managed_probe
#endif
__attribute__((visibility("default")))
int SILPH_MAP_ENTRY(SilphNativeReport *report,uint32_t capacity) {
    int status=silph_native_baseline_managed_probe(report,capacity);if(status)return status;
    Loaded loaded={.report=report};dl_iterate_phdr(modules,&loaded);
    MapContract contract;
    status=map_contract(&loaded.il2cpp,&contract);
    report->managed_status=status;report->managed_created+=contract.metrics.refs_created;report->managed_released+=contract.metrics.refs_released;
    report->managed_remaining+=contract.metrics.live_refs;
    if(!contract.metrics.closed)report->managed_closed=0;
    snprintf(report->detail,sizeof(report->detail),"Map contract: status=%d pointerOffset=%u target=0x%" PRIxPTR " method=0x%" PRIxPTR " klass=0x%" PRIxPTR,
        status,contract.pointer_offset,contract.address,(uintptr_t)contract.method,(uintptr_t)contract.klass);
    return status;
}
