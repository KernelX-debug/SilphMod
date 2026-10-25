static uint64_t extra_clock(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static void extra_log(const char *operation,uint32_t context,uint32_t phase,uint32_t child,uint32_t result){
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO,"SilphResearch","%s context=%u phase=%u sub=%u result=%u",operation,context,phase,child,result);
#else
    (void)operation;(void)context;(void)phase;(void)child;(void)result;
#endif
}
static int extra_claim(Research *s,const SilphSchedulePermit *permit,SilphManagedRef parent,SilphManagedRef child,uint32_t context,uint32_t phase,uint64_t now,bool *started){
    *started=false;SilphManagedRef quest=child.serial?child:parent;SilphReadValue redeem={0},branches={0},rewards={0},result={0};uint32_t n=0;char id[257]={0},sub[257]={0},key[800]={0};
    int status=text(s,parent,"QuestId",id);if(!status&&child.serial)status=text(s,child,"QuestId",sub);
    if(!status){int bytes=snprintf(key,sizeof(key),"story-%u-%u-%zu:%s-%zu:%s",context,phase,strlen(id),id,strlen(sub),sub);if(bytes<0||(size_t)bytes>=sizeof(key))status=SILPH_MANAGED_CONTRACT;}
    if(!status&&!available(s,5,key,now))return 0;
    if(!status){status=call(s,permit,SILPH_RESEARCH_REDEEM,(SilphManagedRef){0},quest,&redeem);s->metrics.operation=220;}
    if(!status&&!redeem.bits)return 0;
    if(!status){s->metrics.operation=221;status=silph_read_field(s->read,quest,"BranchRewards",SILPH_READ_REFERENCE,&branches);}
    if(!status)status=count(s,branches.reference,64,&n);
    if(!status&&n){drop(s,&branches.reference);return 0;}
    if(!status)status=silph_read_field(s->read,quest,"QuestRewards",SILPH_READ_REFERENCE,&rewards);
    if(!status)status=count(s,rewards.reference,32,&n);bool receivable=true;
    for(unsigned i=0;!status&&i<n&&receivable;i++){
        SilphManagedRef reward={0};status=item(s,rewards.reference,i,"QuestRewardProto",&reward);
        if(!status)status=call(s,permit,SILPH_RESEARCH_RECEIVE,s->quests,reward,&result);if(!status&&!result.bits)receivable=false;
        int freed=drop(s,&reward);if(!status)status=freed;
    }
    if(!status&&receivable){
        SilphReadArgument args[]={{.kind=SILPH_READ_REFERENCE,.value.reference=parent},{.kind=SILPH_READ_REFERENCE,.value.reference=child}};
        status=silph_research_invoke(s->read,permit,SILPH_RESEARCH_STORY_CLAIM,s->rpc,args,2,&result);
        if(!status){s->metrics.requests++;status=reserve(s,result.reference,5,key,now,"CompleteQuestOutProto");result.reference=(SilphManagedRef){0};if(!status){*started=true;s->extra_context=context;s->extra_phase=phase;extra_log("claim",context,phase,child.serial?1:0,0);}}
    }
    int a=drop(s,&branches.reference),b=drop(s,&rewards.reference),c=drop(s,&result.reference);return status?status:a?a:b?b:c;
}
static int extra_scan(Research *s,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now,bool capture){
    SilphReadValue proto={0},list={0};uint32_t n=0;bool started=false;uint64_t deadline=extra_clock()+UINT64_C(750000);
    s->extra_due=now+UINT64_C(500000000);int status=call(s,permit,SILPH_RESEARCH_ALL,s->quests,(SilphManagedRef){0},&proto);s->metrics.operation=200;
    if(!status&&!proto.reference.serial){s->extra_due=now+UINT64_C(10000000000);return 0;}
    if(!status){s->metrics.operation=201;status=silph_read_is_type(s->read,proto.reference,"Holoholo.Rpc","QuestsProto");}
    if(!status){s->metrics.operation=202;status=silph_read_field(s->read,proto.reference,s->extra_mode?"Quest":"QuestPokemonEncounter",SILPH_READ_REFERENCE,&list);}
    if(!status){s->metrics.operation=203;status=count(s,list.reference,4096,&n);}
    for(unsigned work=0;!status&&s->extra_index<n&&!started&&work<4;work++){
        if(work&&extra_clock()>=deadline)break;
        SilphManagedRef parent={0};s->metrics.operation=204;status=item(s,list.reference,s->extra_index,s->extra_mode?"QuestProto":"QuestPokemonEncounterProto",&parent);
        if(!s->extra_mode){
            uint32_t type=0;if(!status){s->metrics.operation=205;status=integer(s,parent,"EncounterType",&type);}
            if(!status&&capture&&(type==4||type==10||type==23)){status=begin_reward(s,permit,snapshot,parent,now,&started);if(started)extra_log("encounter",type,0,0,0);}
            s->extra_index++;
        }else{
            uint32_t context=0,phase=0;if(!status){s->metrics.operation=210;status=integer(s,parent,"QuestContext",&context);}
            if(!status&&silph_research_story_context(context)){
                char id[257]={0};SilphReadValue multi={0},children={0};uint32_t child_count=0;
                SilphReadValue phase_value={0};s->extra_context=context;s->metrics.operation=211;status=silph_read_field(s->read,parent,"PhaseNumber",SILPH_READ_I32,&phase_value);
                if(!status&&phase_value.bits>INT32_MAX)status=SILPH_MANAGED_CONTRACT;if(!status)phase=(uint32_t)phase_value.bits;
                if(!status){s->metrics.operation=212;status=text(s,parent,"QuestId",id);}
                if(!status&&(strcmp(id,s->extra_id)||phase!=s->extra_phase)){memcpy(s->extra_id,id,sizeof(id));s->extra_phase=phase;s->extra_child=0;}
                if(!status){s->metrics.operation=213;status=silph_read_call(s->read,parent,"get_MultiPart",SILPH_READ_REFERENCE,NULL,0,&multi);}
                if(!status&&multi.reference.serial){status=silph_read_is_type(s->read,multi.reference,"Holoholo.Rpc","MultiPartQuestProto");
                    if(!status)status=silph_read_field(s->read,multi.reference,"SubQuests",SILPH_READ_REFERENCE,&children);
                    if(!status)status=count(s,children.reference,64,&child_count);}
                if(!status&&s->extra_child<child_count){
                    SilphManagedRef child={0};status=item(s,children.reference,s->extra_child,"QuestProto",&child);
                    if(!status)status=extra_claim(s,permit,parent,child,context,phase,now,&started);s->extra_child++;
                    int freed=drop(s,&child);if(!status)status=freed;
                }else if(!status){status=extra_claim(s,permit,parent,(SilphManagedRef){0},context,phase,now,&started);s->extra_child=0;s->extra_index++;s->extra_id[0]=0;}
                int a=drop(s,&multi.reference),b=drop(s,&children.reference);if(!status)status=a?a:b;
            }else{s->extra_index++;s->extra_child=0;s->extra_id[0]=0;}
        }
        int freed=drop(s,&parent);if(!status)status=freed;
    }
    if(!status&&!started&&s->extra_index>=n){s->extra_index=s->extra_child=0;s->extra_id[0]=0;if(s->extra_mode){s->extra_mode=0;s->extra_due=now+UINT64_C(10000000000);}else s->extra_mode=1;}
    int a=drop(s,&proto.reference),b=drop(s,&list.reference);return status?status:a?a:b;
}
