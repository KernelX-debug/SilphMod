int silph_gift_enumerate(void *memory,SilphManagedRef receiver,uint32_t op,SilphReadValue *out){
    if(!out||op<1||op>4)return SILPH_MANAGED_ARGUMENT;clear_value(out);Read *s=memory;int status=enter_read(s);if(status)return status;
    const char *names[]={"MoveNext","get_Current","Dispose","GetEnumerator"};
    const char *fallback[]={"System.Collections.IEnumerator.MoveNext","System.Collections.IEnumerator.get_Current","System.IDisposable.Dispose","System.Collections.IEnumerable.GetEnumerator"};
    uint32_t kind=op==1?SILPH_READ_BOOL:op==3?SILPH_READ_VOID:SILPH_READ_REFERENCE;
    status=call_internal(s,receiver,NULL,names[op-1],kind,NULL,0,out,NULL);
    if(status==SILPH_MANAGED_CONTRACT)status=call_internal(s,receiver,NULL,fallback[op-1],kind,NULL,0,out,NULL);
    return leave_read(s,status);
}
int silph_gift_invoke(void *memory,const SilphSchedulePermit *permit,uint32_t op,SilphManagedRef receiver,const SilphReadArgument *args,uint32_t count,SilphReadValue *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;clear_value(out);
    const char *names[]={"GetFriendProtos","GetFriendDetail","CheckGiftingStatus","OpenGift","SendGift","GetGiftboxDetails","RemoveGiftbox"};
    const uint32_t counts[]={0,2,1,3,3,1,2};
    if(op<1||op>7||count!=counts[op-1]||(count&&!args)||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;
    status=silph_managed_target(s->managed,receiver,&object);void *k=status?NULL:s->base.object_get_class(object);
    bool friends=op==1||op==2||op==6||op==7;
    void *expected=lookup_class(s,friends?"holo-game.dll":"holo-game-interfaces.dll",friends?"Niantic.Holoholo.Social":"Niantic.Holoholo.Social.Gifting",friends?"IFriendsRpcService":"IGiftingRpcService");
    if(!status&&(!expected||!s->api.class_is_assignable_from(expected,k)))status=SILPH_MANAGED_CONTRACT;
    if(!status&&op!=5)status=call_internal(s,receiver,NULL,names[op-1],op==7?SILPH_READ_VOID:SILPH_READ_REFERENCE,args,count,out,permit);
    if(!status&&op==5){
        const void *m=research_method(s,k,"SendGift",5,false);void *parameters[5]={0};uint64_t id=args[0].value.bits;bool override=false;
        const uint32_t kinds[]={SILPH_READ_U64,SILPH_READ_REFERENCE,SILPH_READ_REFERENCE,SILPH_READ_REFERENCE,SILPH_READ_BOOL};
        if(!m||args[0].kind!=SILPH_READ_U64||!id||!matches_kind(s,s->base.method_get_return_type(m),SILPH_READ_REFERENCE))status=SILPH_MANAGED_CONTRACT;
        for(unsigned i=0;!status&&i<5;i++)if(!matches_kind(s,s->api.method_get_param(m,i),kinds[i]))status=SILPH_MANAGED_CONTRACT;
        for(unsigned i=1;!status&&i<3;i++){
            if(args[i].kind!=SILPH_READ_REFERENCE)status=SILPH_MANAGED_ARGUMENT;
            if(!status)status=silph_managed_target(s->managed,args[i].value.reference,&parameters[i]);
            if(!status&&!s->api.class_is_assignable_from(s->api.class_from_type(s->api.method_get_param(m,i)),s->base.object_get_class(parameters[i])))status=SILPH_MANAGED_CONTRACT;
        }
        void *sticker=lookup_class(s,"holo-protos.dll","Holoholo.Rpc","StickerProto");
        if(!status&&(!sticker||s->api.class_from_type(s->api.method_get_param(m,3))!=sticker))status=SILPH_MANAGED_CONTRACT;
        if(!status&&!silph_schedule_permitted(permit,true))status=SILPH_MANAGED_STATE;
        if(!status){parameters[0]=&id;parameters[4]=&override;void *exception=NULL,*result=s->base.runtime_invoke(m,object,parameters,&exception);
            if(exception)status=SILPH_MANAGED_EXCEPTION;else status=accept_reference(s,s->base.method_get_return_type(m),result,out);}
    }
    return leave_read(s,status);
}
int silph_gift_open_stardust(void *memory,const SilphSchedulePermit *permit,SilphManagedRef handler,SilphManagedRef player,uint64_t gift,SilphReadValue *out){
    if(!out||!gift||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_ARGUMENT;clear_value(out);
    int status=silph_rpc_prepare(memory,SILPH_RPC_GIFT_OPEN,handler);if(status)return status;
    Read *s=memory;status=enter_read(s);if(status)return status;SilphManagedRef payload={0};
    status=fresh_proto(s,"OpenGiftProto",&payload);
    if(!status)status=fresh_reference(s,payload,"PlayerId",player);
    if(!status)status=fresh_scalar(s,payload,"GiftboxId",SILPH_READ_U64,gift);
    if(!status)status=fresh_scalar(s,payload,"ConvertToStardust",SILPH_READ_BOOL,1);
    leave_read(s,status);
    if(!status)status=silph_action_rpc(memory,permit,SILPH_RPC_GIFT_OPEN,handler,payload,out);
    int freed=payload.serial?silph_managed_release(s->managed,payload):0;if(!status)status=freed;return status;
}
