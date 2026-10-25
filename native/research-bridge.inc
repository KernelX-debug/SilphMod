int silph_research_enable(void *memory,SilphManagedResolver resolve,void *context){
    if(!resolve)return SILPH_MANAGED_ARGUMENT;Read *s=memory;int status=enter_read(s);if(status)return status;
    void *symbol=resolve("il2cpp_class_get_nested_types",context),*size=resolve("il2cpp_class_value_size",context);if(!symbol||!size)status=SILPH_MANAGED_ABI_MISMATCH;
    else{memcpy(&s->research_nested,&symbol,sizeof(symbol));memcpy(&s->research_value_size,&size,sizeof(size));}return leave_read(s,status);
}
static void *research_factory(Read *s){
    void *owner=lookup_class(s,"holo-game.dll","Niantic.Holoholo.Quests","QuestMapPokemon");
    if(!owner||!s->research_nested)return NULL;
    void *iterator=NULL,*result=NULL;
    for(unsigned i=0;i<256;i++){void *nested=s->research_nested(owner,&iterator);if(!nested)break;
        const char *name=s->api.class_get_name(nested);if(name&&!strcmp(name,"Factory")){if(result)return NULL;result=nested;}}
    return result;
}
int silph_research_factory_type(void *memory,SilphManagedRef *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));Read *s=memory;int status=enter_read(s);if(status)return status;
    void *klass=research_factory(s),*reflection=klass?s->api.type_get_object(s->base.class_get_type(klass)):NULL;
    if(!reflection)status=SILPH_MANAGED_CONTRACT;else status=silph_managed_retain(s->managed,reflection,false,out);return leave_read(s,status);
}
static bool research_type(Read *s,void *object,const char *space,const char *name){
    void *klass=object?s->base.object_get_class(object):NULL;
    const char *ns=klass?s->api.class_get_namespace(klass):NULL,*nm=klass?s->api.class_get_name(klass):NULL;
    return ns&&nm&&!strcmp(ns,space)&&!strcmp(nm,name);
}
static int research_quest(Read *s,SilphManagedRef quest,SilphManagedRef *id){
    void *object=NULL;int status=silph_managed_target(s->managed,quest,&object);
    if(!status&&!research_type(s,object,"Holoholo.Rpc","QuestProto"))status=SILPH_MANAGED_CONTRACT;
    const void *context=status?NULL:find_field(s,s->base.object_get_class(object),"QuestContext",NULL,NULL,&status);
    uint32_t field=0;if(!status&&!matches_kind(s,s->api.field_get_type(context),SILPH_READ_ENUM32))status=SILPH_MANAGED_CONTRACT;
    if(!status){s->api.field_get_value(object,context,&field);if(field!=2)status=SILPH_MANAGED_STATE;}
    SilphReadValue result={0};const void *id_field=status?NULL:find_field(s,s->base.object_get_class(object),"QuestId",NULL,NULL,&status);
    if(!status&&!matches_kind(s,s->api.field_get_type(id_field),SILPH_READ_REFERENCE))status=SILPH_MANAGED_CONTRACT;
    if(!status){void *text=NULL;s->api.field_get_value(object,id_field,&text);status=accept_reference(s,s->api.field_get_type(id_field),text,&result);}
    if(!status){void *text=NULL;status=silph_managed_target(s->managed,result.reference,&text);int32_t length=status?0:s->base.string_length(text);if(length<1||length>256)status=SILPH_MANAGED_CONTRACT;}
    if(!status){*id=result.reference;result.reference=(SilphManagedRef){0};}release_rpc_ref(s,&result.reference);return status;
}
static const void *research_method(Read *s,void *klass,const char *name,uint32_t count,bool is_static){
    const void *found=NULL;for(unsigned depth=0;klass&&depth<32&&!found;depth++,klass=s->api.class_get_parent(klass)){
        void *iterator=NULL;for(unsigned n=0;n<2048;n++){const void *m=s->base.class_get_methods(klass,&iterator);if(!m)break;const char *nm=s->base.method_get_name(m);uint32_t flags=0;
            if(!nm||strcmp(nm,name)||s->base.method_get_param_count(m)!=count||s->base.method_is_generic(m)
                ||!!(s->base.method_get_flags(m,&flags)&0x10)!=is_static)continue;
            if(found)return NULL;found=m;
        }
    }return found;
}
static int research_nullable(Read *s,const void *method,uint32_t index,uint32_t scalar){
    const void *t=s->api.method_get_param(method,index);void *klass=t?s->api.class_from_type(t):NULL;
    const char *name=klass?s->api.class_get_name(klass):NULL,*space=klass?s->api.class_get_namespace(klass):NULL;
    if(!klass||!s->api.class_is_valuetype(klass)||!name||strcmp(name,"Nullable`1")||!space||strcmp(space,"System"))return SILPH_MANAGED_CONTRACT;
    int status=0;const void *has=find_field(s,klass,"hasValue",NULL,NULL,&status),*value=find_field(s,klass,"value",NULL,NULL,&status);
    if(status||!matches_kind(s,s->api.field_get_type(has),SILPH_READ_BOOL)||!matches_kind(s,s->api.field_get_type(value),scalar))return SILPH_MANAGED_CONTRACT;
    uint32_t alignment=0;int32_t size=s->research_value_size?s->research_value_size(klass,&alignment):0;
    return size>=8&&size<=32&&alignment&&alignment<=8?0:SILPH_MANAGED_CONTRACT;
}
static int research_nullable_invoke(Read *s,const void *method,void *object,uint32_t kind,SilphReadArgument *args,uint32_t count,uint32_t nullable,SilphReadValue *out,const SilphSchedulePermit *permit){
    if(!method||count>4||nullable>=count||s->base.method_get_param_count(method)!=count||!matches_kind(s,s->base.method_get_return_type(method),kind))return SILPH_MANAGED_CONTRACT;
    uint64_t empty[4]={0},scalars[4]={0};void *parameters[4]={0};int status=0;
    for(unsigned i=0;!status&&i<count;i++){
        if(i==nullable){parameters[i]=empty;continue;}
        const void *type=s->api.method_get_param(method,i);if(!matches_kind(s,type,args[i].kind)){status=SILPH_MANAGED_CONTRACT;break;}
        if(args[i].kind==SILPH_READ_REFERENCE){status=silph_managed_target(s->managed,args[i].value.reference,&parameters[i]);if(!status&&!s->api.class_is_assignable_from(s->api.class_from_type(type),s->base.object_get_class(parameters[i])))status=SILPH_MANAGED_CONTRACT;}
        else if(args[i].kind==SILPH_READ_U64){scalars[i]=args[i].value.bits;parameters[i]=&scalars[i];}else status=SILPH_MANAGED_CONTRACT;
    }
    if(!status&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
    if(!status){void *exception=NULL,*result=s->base.runtime_invoke(method,object,parameters,&exception);
        if(exception)status=SILPH_MANAGED_EXCEPTION;
        else if(kind==SILPH_READ_REFERENCE)status=accept_reference(s,s->base.method_get_return_type(method),result,out);
        else if(!result||s->base.object_get_class(result)!=s->api.class_from_type(s->base.method_get_return_type(method)))status=SILPH_MANAGED_CONTRACT;
        else{SilphManagedRef held={0};status=silph_managed_retain(s->managed,result,false,&held);void *value=NULL;
            if(!status)status=silph_managed_target(s->managed,held,&value);void *scalar=status?NULL:s->base.object_unbox(value);
            if(!status&&!scalar)status=SILPH_MANAGED_CONTRACT;if(!status){memcpy(&out->bits,scalar,1);if(out->bits>1)status=SILPH_MANAGED_CONTRACT;}int freed=release_rpc_ref(s,&held);if(!status)status=freed;}
    }
    if(status)clear_value(out);return status;
}
int silph_research_invoke(void *memory,const SilphSchedulePermit *permit,uint32_t operation,SilphManagedRef receiver,const SilphReadArgument *input,uint32_t count,SilphReadValue *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;clear_value(out);if(operation<1||operation>13||count>2||(count&&!input)||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *s=memory;int status=enter_read(s);if(status)return status;
    const char *names[]={"GetQuestDataOfType","get_ExclusiveQuestCompletionInProgress","CanReceiveReward","CanRedeem","CompleteQuest","RemoveQuest","GetDanglingEncounterOfType","BeginQuestEncounter","Create","get_IsStampCardFull","get_StampCardData","CompleteStampCard","GetDanglingEncounterOfType"};
    uint32_t needed[]={0,0,1,1,1,1,0,1,2,0,0,0,0};if(count!=needed[operation-1])return leave_read(s,SILPH_MANAGED_ARGUMENT);
    uint32_t phase=40;bool statik=operation==SILPH_RESEARCH_REDEEM;void *object=NULL,*klass=NULL;
    bool stamp=operation==10||operation==11;const char *iface=stamp?"IQuestStampCardService":operation==5||operation==6||operation==8||operation==12?"IQuestRpcService":"IQuestService";
    if(statik)klass=lookup_class(s,"holo-game.dll","Niantic.Holoholo.Quests","QuestProtoExt");
    else{status=silph_managed_target(s->managed,receiver,&object);klass=status?NULL:s->base.object_get_class(object);
        void *expected=operation==9?research_factory(s):lookup_class(s,stamp?"holo-game-interfaces.dll":"holo-game.dll","Niantic.Holoholo.Quests",iface);
        if(!status&&(!expected||!s->api.class_is_assignable_from(expected,klass)))status=SILPH_MANAGED_CONTRACT;
    }
    uint32_t nargs=operation==1?1:operation==4||operation==7||operation==9||operation==13?2:operation==5?4:count;
    const void *method=NULL;if(!status&&(operation==4||operation==5)){phase=41;method=research_method(s,klass,names[operation-1],nargs,statik);if(!method)status=SILPH_MANAGED_CONTRACT;}
    SilphReadArgument args[4]={{0}};for(unsigned i=0;i<count;i++)args[i]=input[i];SilphManagedRef id={0},empty={0};
    if(!status&&operation==1)args[0]=(SilphReadArgument){.kind=SILPH_READ_ENUM32,.value.bits=2};
    if(!status&&(operation==5||operation==6)){
        status=research_quest(s,input[0].value.reference,&id);args[0]=(SilphReadArgument){.kind=SILPH_READ_REFERENCE,.value.reference=id};
    }
    if(!status&&(operation==5||operation==7||operation==13)){void *value=s->base.string_new_len("",0);status=value?silph_managed_retain(s->managed,value,false,&empty):SILPH_MANAGED_CAPACITY;}
    if(!status&&(operation==4||operation==5)){phase=42;status=research_nullable(s,method,operation==4?1:2,operation==4?SILPH_READ_I64:SILPH_READ_I32);}
    if(!status&&operation==5){args[1]=(SilphReadArgument){.kind=SILPH_READ_REFERENCE,.value.reference=empty};args[3]=(SilphReadArgument){.kind=SILPH_READ_U64};}
    if(!status&&(operation==7||operation==13)){args[0]=(SilphReadArgument){.kind=SILPH_READ_ENUM32,.value.bits=operation==13?5:6};args[1]=(SilphReadArgument){.kind=SILPH_READ_REFERENCE,.value.reference=empty};}
    uint32_t result_kind=operation==2||operation==3||operation==4||operation==10?SILPH_READ_BOOL:SILPH_READ_REFERENCE;
    if(!status){phase=43;if(method)status=research_nullable_invoke(s,method,object,result_kind,args,nargs,operation==4?1:2,out,permit);
        else status=call_internal(s,receiver,NULL,names[operation-1],result_kind,args,nargs,out,permit);}
    int a=release_rpc_ref(s,&id),b=release_rpc_ref(s,&empty);if(!status){phase=44;status=a?a:b;}if(status)out->bits=phase;return leave_read(s,status);
}
int silph_research_destroy(void *memory,SilphManagedRef ref){
    Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;status=silph_managed_target(s->managed,ref,&object);
    if(!status&&!research_type(s,object,"Niantic.Holoholo.Quests","QuestMapPokemon"))status=SILPH_MANAGED_CONTRACT;
    SilphReadValue destroyed={0},none={0};if(!status)status=call_internal(s,ref,NULL,"get_Destroyed",SILPH_READ_BOOL,NULL,0,&destroyed,NULL);
    if(!status&&!destroyed.bits)status=call_internal(s,ref,NULL,"Destroy",SILPH_READ_VOID,NULL,0,&none,NULL);return leave_read(s,status);
}
