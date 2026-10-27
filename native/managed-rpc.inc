typedef struct { int32_t id;const char *request,*response; } RpcSchema;
static const RpcSchema rpc_schemas[]={
    {2005,"GetPokestopEncounterProto","GetPokestopEncounterOutProto"},
    {2006,"EncounterPokestopEncounterProto","EncounterPokestopEncounterOutProto"},
    {154,"UseItemEncounterProto","UseItemEncounterOutProto"},
    {103,"CatchPokemonProto","CatchPokemonOutProto"},
    {101,"FortSearchProto","FortSearchOutProto"},
    {137,"RecycleItemProto","RecycleItemOutProto"},
    {1350,"BuddyMapProto","BuddyMapOutProto"},
    {152,"SetBuddyPokemonProto","SetBuddyPokemonOutProto"},
    {951,"OpenGiftProto","OpenGiftOutProto"},
    {3034,"ClaimEventPassRewardsRequestProto","ClaimEventPassRewardsResponseProto"},
    {3035,"ClaimEventPassRewardsRequestProto","ClaimEventPassRewardsResponseProto"},
    {103,"CatchPokemonProto","CatchPokemonOutProto"}
};
int silph_rpc_enable(void *memory,SilphManagedResolver resolve,void *context){
    if(!resolve)return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;
    void *method_object=resolve("il2cpp_method_get_object",context),*method_reflection=resolve("il2cpp_method_get_from_reflection",context);
    void *array=resolve("il2cpp_array_new",context),*length=resolve("il2cpp_array_length",context);
    if(!method_object||!method_reflection||!array||!length)status=SILPH_MANAGED_ABI_MISMATCH;
    if(!status){
        memcpy(&s->method_get_object,&method_object,sizeof(method_object));memcpy(&s->method_get_from_reflection,&method_reflection,sizeof(method_reflection));
        memcpy(&s->array_new,&array,sizeof(array));memcpy(&s->array_length,&length,sizeof(length));
    }
    return leave_read(s,status);
}
static int release_rpc_ref(Read *s,SilphManagedRef *ref){
    if(!ref->serial)return 0;int status=silph_managed_release(s->managed,*ref);if(!status)memset(ref,0,sizeof(*ref));return status;
}
static int rpc_handler(Read *s,SilphManagedRef ref,void **object,void **klass){
    int status=silph_managed_target(s->managed,ref,object);*klass=status?NULL:s->base.object_get_class(*object);
    void *expected=lookup_class(s,"holo-game.dll","Niantic.Holoholo.Rpc","IRpcHandler");
    if(!status&&(!expected||!*klass||!s->api.class_is_assignable_from(expected,*klass)))status=SILPH_MANAGED_CONTRACT;
    return status;
}
int silph_rpc_prepare(void *memory,uint32_t operation,SilphManagedRef handler){
    if(operation<1||operation>SILPH_RPC_COUNT)return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;
    if(!s->method_get_object||!s->method_get_from_reflection||!s->array_new||!s->array_length)return leave_read(s,SILPH_MANAGED_STATE);
    unsigned slot=operation-1;void *object=NULL,*actual=NULL;status=rpc_handler(s,handler,&object,&actual);
    if(!status&&s->rpc_methods[slot])return leave_read(s,s->rpc_classes[slot]==actual?0:SILPH_MANAGED_STALE);
    const void *definition=NULL;
    for(unsigned depth=0;!status&&!definition&&actual&&depth<32;depth++,actual=s->api.class_get_parent(actual)){
        void *iterator=NULL;
        for(unsigned i=0;;i++){
            if(i>=2048){status=SILPH_MANAGED_CONTRACT;break;}
            const void *method=s->base.class_get_methods(actual,&iterator);if(!method)break;
            const char *name=s->base.method_get_name(method);uint32_t flags=0;
            if(!name||strcmp(name,"SendRpc")||s->base.method_get_param_count(method)!=5
                ||!s->base.method_is_generic(method)||s->base.method_is_inflated(method)||(s->base.method_get_flags(method,&flags)&0x10))continue;
            if(!matches_kind(s,s->api.method_get_param(method,0),SILPH_READ_I32)
                ||!matches_kind(s,s->api.method_get_param(method,2),SILPH_READ_I32)
                ||!matches_kind(s,s->api.method_get_param(method,3),SILPH_READ_I32)
                ||!matches_kind(s,s->api.method_get_param(method,4),SILPH_READ_BOOL))continue;
            if(definition){status=SILPH_MANAGED_CONTRACT;break;}definition=method;
        }
    }
    if(!status&&!definition)status=SILPH_MANAGED_CONTRACT;
    SilphManagedRef reflection={0},array={0},types[2]={{0}},closed={0};SilphReadValue value={0};
    void *request_class=lookup_class(s,"holo-protos.dll","Holoholo.Rpc",rpc_schemas[slot].request);
    void *response_class=lookup_class(s,"holo-protos.dll","Holoholo.Rpc",rpc_schemas[slot].response);
    void *type_class=lookup_class(s,"mscorlib.dll","System","Type");
    if(!status&&(!request_class||!response_class||!type_class))status=SILPH_MANAGED_CONTRACT;
    if(!status){void *v=s->method_get_object(definition,s->api.method_get_class(definition));status=v?silph_managed_retain(s->managed,v,false,&reflection):SILPH_MANAGED_CONTRACT;}
    if(!status)status=call_internal(s,reflection,NULL,"GetGenericArguments",SILPH_READ_REFERENCE,NULL,0,&value,NULL);
    if(!status){void *v=NULL;status=silph_managed_target(s->managed,value.reference,&v);if(!status&&s->array_length(v)!=2)status=SILPH_MANAGED_CONTRACT;}
    int freed=release_rpc_ref(s,&value.reference);if(!status)status=freed;
    if(!status){void *v=s->array_new(type_class,2);status=v?silph_managed_retain(s->managed,v,false,&array):SILPH_MANAGED_CAPACITY;}
    void *classes[]={request_class,response_class};
    for(unsigned i=0;!status&&i<2;i++){
        void *v=s->api.type_get_object(s->base.class_get_type(classes[i]));
        status=v?silph_managed_retain(s->managed,v,false,&types[i]):SILPH_MANAGED_CONTRACT;
        SilphReadArgument arguments[]={{.kind=SILPH_READ_REFERENCE,.value.reference=types[i]},{.kind=SILPH_READ_I32,.value.bits=i}};
        if(!status)status=call_internal(s,array,NULL,"SetValue",SILPH_READ_VOID,arguments,2,&value,NULL);
    }
    SilphReadArgument argument={.kind=SILPH_READ_REFERENCE,.value.reference=array};
    if(!status)status=call_internal(s,reflection,NULL,"MakeGenericMethod",SILPH_READ_REFERENCE,&argument,1,&value,NULL);
    if(!status){closed=value.reference;memset(&value.reference,0,sizeof(value.reference));}
    const void *method=NULL;
    if(!status){void *v=NULL;status=silph_managed_target(s->managed,closed,&v);if(!status)method=s->method_get_from_reflection(v);}
    SilphReadArgument signature[]={{.kind=SILPH_READ_I32},{.kind=SILPH_READ_REFERENCE},{.kind=SILPH_READ_I32},{.kind=SILPH_READ_I32},{.kind=SILPH_READ_BOOL}};
    if(!status){
        uint32_t flags=0;const char *name=method?s->base.method_get_name(method):NULL;
        if(!method||!name||strcmp(name,"SendRpc")||s->base.method_get_param_count(method)!=5||!s->base.method_is_inflated(method)
            ||(s->base.method_get_flags(method,&flags)&0x10)||!matches_kind(s,s->base.method_get_return_type(method),SILPH_READ_REFERENCE)
            ||!s->api.class_is_assignable_from(s->api.method_get_class(method),s->base.object_get_class(object)))status=SILPH_MANAGED_CONTRACT;
        for(unsigned i=0;!status&&i<5;i++)if(!matches_kind(s,s->api.method_get_param(method,i),signature[i].kind))status=SILPH_MANAGED_CONTRACT;
        if(!status&&s->api.class_from_type(s->api.method_get_param(method,1))!=request_class)status=SILPH_MANAGED_CONTRACT;
    }
    if(!status){s->rpc_methods[slot]=method;s->rpc_classes[slot]=s->base.object_get_class(object);s->rpc_roots[slot]=closed;memset(&closed,0,sizeof(closed));}
    SilphManagedRef *refs[]={&reflection,&array,&types[0],&types[1],&closed,&value.reference};
    for(unsigned i=0;i<6;i++){freed=release_rpc_ref(s,refs[i]);if(!status)status=freed;}
    return leave_read(s,status);
}
int silph_action_rpc(void *memory,const SilphSchedulePermit *permit,uint32_t operation,SilphManagedRef handler,SilphManagedRef request,SilphReadValue *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;clear_value(output);
    if(operation<1||operation>SILPH_RPC_COUNT)return SILPH_MANAGED_ARGUMENT;
    if(!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *s=memory;int status=enter_read(s);if(status)return status;
    unsigned slot=operation-1;void *object=NULL,*actual=NULL,*payload=NULL;
    status=rpc_handler(s,handler,&object,&actual);
    if(!status&&(!s->rpc_methods[slot]||s->rpc_classes[slot]!=actual))status=SILPH_MANAGED_STATE;
    if(!status)status=silph_managed_target(s->managed,request,&payload);
    void *expected=lookup_class(s,"holo-protos.dll","Holoholo.Rpc",rpc_schemas[slot].request);
    if(!status&&(!expected||s->base.object_get_class(payload)!=expected))status=SILPH_MANAGED_CONTRACT;
    SilphReadArgument args[]={{.kind=SILPH_READ_I32,.value.bits=(uint32_t)rpc_schemas[slot].id},
        {.kind=SILPH_READ_REFERENCE,.value.reference=request},{.kind=SILPH_READ_I32,.value.bits=1000},
        {.kind=SILPH_READ_I32,.value.bits=10000},{.kind=SILPH_READ_BOOL,.value.bits=0}};
    if(!status)status=invoke_checked(s,s->rpc_methods[slot],object,SILPH_READ_REFERENCE,args,5,output,permit);
    return leave_read(s,status);
}
