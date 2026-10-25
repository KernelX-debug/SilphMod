int silph_pass_enumerate(void *memory,SilphManagedRef receiver,uint32_t operation,SilphReadValue *out){
    int status=silph_gift_enumerate(memory,receiver,operation,out);if(status!=SILPH_MANAGED_CONTRACT)return status;
    Read *s=memory;status=enter_read(s);if(status)return status;void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
    void *owner=lookup_class(s,"holo-game.dll","Niantic.Holoholo.UI.TodayView.EventPass","EventPassService"),*expected=NULL,*iterator=NULL;
    for(unsigned i=0;owner&&s->research_nested&&i<256;i++){void *nested=s->research_nested(owner,&iterator);if(!nested)break;const char *name=s->api.class_get_name(nested);if(name&&!strcmp(name,"fyr")){if(expected){status=SILPH_MANAGED_CONTRACT;break;}expected=nested;}}
    if(!status&&(!expected||s->base.object_get_class(object)!=expected))status=SILPH_MANAGED_CONTRACT;
    const char *names[]={"MoveNext","cfkm","cfkj","cfkn"};uint32_t kind=operation==1?SILPH_READ_BOOL:operation==3?SILPH_READ_VOID:SILPH_READ_REFERENCE;
    if(!status)status=call_internal(s,receiver,NULL,names[operation-1],kind,NULL,0,out,NULL);return leave_read(s,status);
}
int silph_pass_invoke(void *memory,const SilphSchedulePermit *permit,uint32_t operation,SilphManagedRef receiver,const SilphReadArgument *args,uint32_t count,SilphReadValue *out){
    if(!out||operation<1||operation>5||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_ARGUMENT;clear_value(out);
    const char *names[]={"GetCurrentInventoryEventPasses","GetUnclaimedRewardsCount","EnumerateUnclaimedEncounters","IsRewardClaimed","IsPremiumTrackActive"};const uint32_t counts[]={0,1,1,3,1};
    if(count!=counts[operation-1]||(count&&!args))return SILPH_MANAGED_ARGUMENT;Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;
    status=silph_managed_target(s->managed,receiver,&object);void *expected=lookup_class(s,"holo-game.dll","Niantic.Holoholo.UI.TodayView.EventPass","IEventPassService");
    if(!status&&(!expected||!s->api.class_is_assignable_from(expected,s->base.object_get_class(object))))status=SILPH_MANAGED_CONTRACT;
    uint32_t kind=operation>=4?SILPH_READ_BOOL:SILPH_READ_REFERENCE;
    if(!status)status=call_internal(s,receiver,NULL,names[operation-1],kind,args,count,out,permit);return leave_read(s,status);
}
int silph_pass_claim(void *memory,const SilphSchedulePermit *permit,SilphManagedRef handler,SilphManagedRef pass,uint32_t track,uint32_t rank,bool all,SilphReadValue *out){
    if(!out||!silph_schedule_permitted(permit,true)||(!all&&(track<1||track>2||rank>10000)))return SILPH_MANAGED_ARGUMENT;clear_value(out);
    uint32_t operation=all?SILPH_RPC_PASS_ALL:SILPH_RPC_PASS_SLOT;int status=silph_rpc_prepare(memory,operation,handler);if(status)return status;
    Read *s=memory;status=enter_read(s);if(status)return status;SilphManagedRef request={0},slot={0};SilphReadValue slots={0},ignored={0};
    status=fresh_proto(s,"ClaimEventPassRewardsRequestProto",&request);if(!status)status=fresh_reference(s,request,"PassId",pass);
    if(!status)status=fresh_scalar(s,request,"ClaimAllRewards",SILPH_READ_BOOL,all);
    if(!status&&!all){
        void *object=NULL;status=silph_managed_target(s->managed,request,&object);const void *field=status?NULL:find_field(s,s->base.object_get_class(object),"RewardSlots",NULL,NULL,&status);void *list=NULL;
        if(!status&&matches_kind(s,s->api.field_get_type(field),SILPH_READ_REFERENCE)){s->api.field_get_value(object,field,&list);status=accept_reference(s,s->api.field_get_type(field),list,&slots);}else if(!status)status=SILPH_MANAGED_CONTRACT;
        const void *item=!status&&list?research_method(s,s->base.object_get_class(list),"get_Item",1,false):NULL;void *klass=item?s->api.class_from_type(s->base.method_get_return_type(item)):NULL;
        const char *name=klass?s->api.class_get_name(klass):NULL;if(!status&&(!item||!klass||!name||strcmp(name,"ClaimRewardsSlotProto")||!matches_kind(s,s->api.method_get_param(item,0),SILPH_READ_I32)||!matches_kind(s,s->base.method_get_return_type(item),SILPH_READ_REFERENCE)))status=SILPH_MANAGED_CONTRACT;
        void *created=status?NULL:s->object_new(klass);if(!status&&!created)status=SILPH_MANAGED_CAPACITY;
        if(!status)status=silph_managed_retain(s->managed,created,false,&slot);if(!status)status=call_internal(s,slot,NULL,".ctor",SILPH_READ_VOID,NULL,0,&ignored,NULL);
        if(!status)status=fresh_scalar(s,slot,"Track",SILPH_READ_ENUM32,track);if(!status)status=fresh_scalar(s,slot,"Rank",SILPH_READ_I32,rank);
        SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=slot};if(!status)status=call_internal(s,slots.reference,NULL,"Add",SILPH_READ_VOID,&arg,1,&ignored,permit);
    }
    leave_read(s,status);if(!status)status=silph_action_rpc(memory,permit,operation,handler,request,out);
    int a=release_rpc_ref(s,&slot),b=release_rpc_ref(s,&slots.reference),c=release_rpc_ref(s,&request);return status?status:a?a:b?b:c;
}
