int silph_buddy_care_invoke(void *memory,const SilphSchedulePermit *p,uint32_t op,SilphManagedRef receiver,const SilphReadArgument *args,uint32_t count,SilphReadValue *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;clear_value(out);
    const char *names[]={"Feed","RequestBuddyStats","GetAllMegaEvoBranches","GetPokemonSettings","CanAffordMegaEvolution","MeetsEvoMoveRequirement","CanTempEvolve"};
    const uint32_t counts[]={2,0,1,2,3,1,1};
    if(op<1||op>7||count!=counts[op-1]||(count&&!args)||!silph_schedule_permitted(p,true))return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int r=enter_read(s);if(r)return r;void *object=NULL;r=silph_managed_target(s->managed,receiver,&object);
    const char *space=op<=2?"Niantic.Holoholo.Buddy":(op<=4||op==7)?"Holoholo.Rpc":"Niantic.Holoholo.Services";
    const char *name=op<=2?"IBuddyRpcService":(op<=4||op==7)?"PokemonProto":"IMegaEvoCostService";
    void *type=lookup_class(s,op<=2?"holo-game.dll":(op<=4||op==7)?"holo-protos.dll":"holo-game-interfaces.dll",space,name);
    if(!r&&(!type||!s->api.class_is_assignable_from(type,s->base.object_get_class(object))))r=SILPH_MANAGED_CONTRACT;
    if(!r)r=call_internal(s,receiver,NULL,names[op-1],op>=5?SILPH_READ_BOOL:SILPH_READ_REFERENCE,args,count,out,p);
    return leave_read(s,r);
}
int silph_buddy_care_cost(void *memory,const SilphSchedulePermit *p,SilphManagedRef service,SilphManagedRef pokemon,SilphManagedRef branch,int32_t *cost){
    if(!cost||!silph_schedule_permitted(p,true))return SILPH_MANAGED_ARGUMENT;*cost=-1;Read *s=memory;int r=enter_read(s);if(r)return r;
    void *object=NULL,*parameters[2]={0};r=silph_managed_target(s->managed,service,&object);
    void *expected=lookup_class(s,"holo-game-interfaces.dll","Niantic.Holoholo.Services","IMegaEvoCostService");
    if(!r&&(!expected||!s->api.class_is_assignable_from(expected,s->base.object_get_class(object))))r=SILPH_MANAGED_CONTRACT;
    const void *method=r?NULL:research_method(s,s->base.object_get_class(object),"CalculateMegaEnergyCost",2,false);
    if(!r&&(!method||!matches_kind(s,s->base.method_get_return_type(method),SILPH_READ_BOXED)))r=SILPH_MANAGED_CONTRACT;
    SilphManagedRef refs[]={pokemon,branch};
    for(unsigned i=0;!r&&i<2;i++){
        const void *type=s->api.method_get_param(method,i);r=silph_managed_target(s->managed,refs[i],&parameters[i]);
        if(!r&&(!matches_kind(s,type,SILPH_READ_REFERENCE)||!s->api.class_is_assignable_from(s->api.class_from_type(type),s->base.object_get_class(parameters[i]))))r=SILPH_MANAGED_CONTRACT;
    }
    if(!r&&!silph_schedule_permitted(p,true))r=SILPH_MANAGED_STATE;
    if(!r){void *exception=NULL,*result=s->base.runtime_invoke(method,object,parameters,&exception);
        if(exception)r=SILPH_MANAGED_EXCEPTION;
        else if(result){
            SilphManagedRef held={0};r=silph_managed_retain(s->managed,result,false,&held);void *value_object=NULL;
            if(!r)r=silph_managed_target(s->managed,held,&value_object);
            void *actual=r?NULL:s->base.object_get_class(value_object);
            if(!r&&s->base.type_get_type(s->base.class_get_type(actual))!=SILPH_READ_I32)r=SILPH_MANAGED_CONTRACT;
            if(!r){void *value=s->base.object_unbox(value_object);if(!value)r=SILPH_MANAGED_CONTRACT;else{memcpy(cost,value,4);if(*cost<0||*cost>1000000)r=SILPH_MANAGED_CONTRACT;}}
            int d=release_rpc_ref(s,&held);if(!r)r=d;
        }
    }
    return leave_read(s,r);
}
int silph_buddy_care_mega(void *memory,const SilphSchedulePermit *p,SilphManagedRef bag,SilphManagedRef pokemon,uint64_t id,uint32_t form,SilphReadValue *out){
    if(!out||!id||!form||!silph_schedule_permitted(p,true))return SILPH_MANAGED_ARGUMENT;clear_value(out);Read *s=memory;int r=enter_read(s);if(r)return r;
    SilphManagedRef proto={0};void *object=NULL;r=silph_managed_target(s->managed,bag,&object);
    void *expected=lookup_class(s,"holo-game-interfaces.dll","Niantic.Holoholo","IPokemonBag");
    if(!r&&(!expected||!s->api.class_is_assignable_from(expected,s->base.object_get_class(object))))r=SILPH_MANAGED_CONTRACT;
    if(!r)r=fresh_proto(s,"MegaEvolvePokemonProto",&proto);
    if(!r)r=fresh_scalar(s,proto,"PokemonId",SILPH_READ_U64,id);
    if(!r)r=fresh_scalar(s,proto,"TempEvoId",SILPH_READ_ENUM32,form);
    if(!r)r=fresh_scalar(s,proto,"Preview",SILPH_READ_BOOL,0);
    SilphReadArgument args[]={{.kind=SILPH_READ_REFERENCE,.value.reference=pokemon},{.kind=SILPH_READ_REFERENCE,.value.reference=proto}};
    if(!r)r=call_internal(s,bag,NULL,"MegaEvolvePokemon",SILPH_READ_REFERENCE,args,2,out,p);
    int d=release_rpc_ref(s,&proto);return leave_read(s,r?r:d);
}
