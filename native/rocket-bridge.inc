#include "rocket-bridge.h"
#include <android/log.h>
int silph_rocket_invoke(void *memory,const SilphSchedulePermit *permit,uint32_t operation,SilphManagedRef receiver,
    const SilphReadArgument *args,uint32_t count,SilphReadValue *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;clear_value(out);
    if(operation<1||operation>
#ifdef SILPH_PILOT_ROCKET
        28
#else
        26
#endif
        ||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;
    Read *s=memory;int status=enter_read(s);if(status)return status;
#ifdef SILPH_PILOT_ROCKET
    if(operation==RK_REWARD_DISMISS){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,"Niantic.Holoholo.Incident","IncidentCombatResolveGuiController")))status=SILPH_MANAGED_CONTRACT;
        const void *state=status?NULL:find_field(s,s->base.object_get_class(object),"currentState",NULL,NULL,&status);int32_t active=0;
        if(!status&&!matches_kind(s,s->api.field_get_type(state),SILPH_READ_ENUM32))status=SILPH_MANAGED_CONTRACT;
        if(!status)s->api.field_get_value(object,state,&active);
        uintptr_t native=0;const void *alive=status?NULL:find_field(s,s->base.object_get_class(object),"m_CachedPtr",NULL,NULL,&status);
        if(!status&&(s->api.type_is_byref(s->api.field_get_type(alive))||s->base.type_get_type(s->api.field_get_type(alive))!=24))status=SILPH_MANAGED_CONTRACT;
        if(!status)s->api.field_get_value(object,alive,&native);
        if(!status&&active==2&&native)status=call_internal(s,receiver,NULL,"Dismiss",SILPH_READ_VOID,NULL,0,out,permit);
        return leave_read(s,status);
    }
    if(operation==RK_REWARD_COMPLETE){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count!=2||args[0].kind!=SILPH_READ_U64||!args[0].value.bits||args[1].kind!=SILPH_READ_ENUM32||args[1].value.bits>3||!research_type(s,object,"Niantic.Holoholo.Incident","IncidentService")))status=SILPH_MANAGED_CONTRACT;
        const void *field=status?NULL:find_field(s,s->base.object_get_class(object),NULL,"Holoholo.Rpc","InvasionEncounterOutProto",&status);void *cached=NULL;
        if(!status)s->api.field_get_value(object,field,&cached);
        if(!status&&(!cached||!research_type(s,cached,"Holoholo.Rpc","InvasionEncounterOutProto")))status=SILPH_MANAGED_NOT_READY;
        uint64_t encounter=0;const void *id=status?NULL:find_field(s,s->base.object_get_class(cached),"EncounterId",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(id),SILPH_READ_U64))status=SILPH_MANAGED_CONTRACT;
        if(!status){s->api.field_get_value(cached,id,&encounter);if(encounter!=args[0].value.bits)status=SILPH_MANAGED_STALE;}
        if(!status)status=call_internal(s,receiver,NULL,"betq",SILPH_READ_VOID,&args[1],1,out,permit);
        return leave_read(s,status);
    }
#endif
    if(operation==RK_PLAYER_ALIAS){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,"Niantic.Holoholo.Combat.UI","CombatVNextGuiController")))status=SILPH_MANAGED_CONTRACT;
        SilphManagedRef label={0};if(!status)status=silph_managed_ascii(s->managed,"SilphMod",8,&label);
        SilphReadArgument values[]={{.kind=SILPH_READ_REFERENCE,.value.reference=label},{.kind=SILPH_READ_BOOL,.value.bits=1}};
        if(!status)status=call_internal(s,receiver,NULL,"DisplayNameplateText",SILPH_READ_VOID,values,2,out,permit);
        int freed=release_rpc_ref(s,&label);return leave_read(s,status?status:freed);
    }
    if(operation==RK_FAST_GUI){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);if(status)return leave_read(s,status);
        if(count)return leave_read(s,SILPH_MANAGED_ARGUMENT);
        const char *fields[3]={0};unsigned total=0;bool animate=false;
        if(research_type(s,object,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")){fields[0]="backgroundTransitionWaitDurationSec";fields[1]="versusScreenDurationSec";fields[2]="combatTransitionDurationSec";total=3;}
        else if(research_type(s,object,"Niantic.Holoholo.Combat.NpcTrainers","NpcTrainerInterstitialGui")){fields[0]="backdropFadeInDuration";total=1;}
        else if(research_type(s,object,"Niantic.Holoholo.Incident","IncidentInvasionDialogGui")){fields[0]="bubbleDelaySeconds";total=1;}
        else if(research_type(s,object,"Niantic.Holoholo.Incident","IncidentCombatResolveGuiController"))animate=true;
        else return leave_read(s,SILPH_MANAGED_NOT_READY);
        const void *validated[3]={0};float previous[3]={0};
        for(unsigned i=0;!status&&i<total;i++){validated[i]=find_field(s,s->base.object_get_class(object),fields[i],NULL,NULL,&status);if(!status&&!matches_kind(s,s->api.field_get_type(validated[i]),SILPH_READ_F32))status=SILPH_MANAGED_CONTRACT;if(!status){s->api.field_get_value(object,validated[i],&previous[i]);if(!isfinite(previous[i])||previous[i]<0||previous[i]>120)status=SILPH_MANAGED_CONTRACT;}}
        if(!status)for(unsigned i=0;i<total;i++){float value=0;s->field_set_value(object,validated[i],&value);}
        if(!status&&animate){SilphReadValue animator={0};const void *field=find_field(s,s->base.object_get_class(object),"animator",NULL,NULL,&status);if(!status){void *value=NULL;s->api.field_get_value(object,field,&value);status=accept_reference(s,s->api.field_get_type(field),value,&animator);}
            if(!status&&!animator.reference.serial)status=SILPH_MANAGED_NOT_READY;
            if(!status){float speed=16;SilphReadArgument argument={.kind=SILPH_READ_F32};memcpy(&argument.value.bits,&speed,sizeof(speed));status=call_internal(s,animator.reference,NULL,"set_speed",SILPH_READ_VOID,&argument,1,out,permit);}
            int freed=release_rpc_ref(s,&animator.reference);if(!status)status=freed;
        }
        return leave_read(s,status);
    }
    if(operation==RK_PARTY_RECOMMEND){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget")))status=SILPH_MANAGED_CONTRACT;
        SilphReadValue service={0},league={0},scroll={0},page={0},party={0};
        const char *fields[]={"dnoc","dnoq","dynoScrollRect"};SilphReadValue *values[]={&service,&league,&scroll};
        for(unsigned i=0;!status&&i<3;i++){
            const void *field=find_field(s,s->base.object_get_class(object),fields[i],NULL,NULL,&status);
            if(!status){void *v=NULL;s->api.field_get_value(object,field,&v);status=accept_reference(s,s->api.field_get_type(field),v,values[i]);}
            if(!status&&i!=1&&!values[i]->reference.serial)status=SILPH_MANAGED_NOT_READY;
        }
        if(!status)status=call_internal(s,scroll.reference,NULL,"CurrentPageGui",SILPH_READ_REFERENCE,NULL,0,&page,permit);
        if(!status&&!page.reference.serial)status=SILPH_MANAGED_NOT_READY;
        if(!status)status=silph_managed_target(s->managed,page.reference,&object);
        if(!status&&!research_type(s,object,"Niantic.Holoholo.Gym","CombatPrepPartyPanel"))status=SILPH_MANAGED_CONTRACT;
        if(!status&&!league.reference.serial){
            SilphReadValue gate={0},rolled_back={0};SilphManagedRef key={0};const char *template="NPC_LEAGUE_SUFFIX_ROLL_BACK";
            void *widget=NULL;status=silph_managed_target(s->managed,receiver,&widget);
            const void *field=status?NULL:find_field(s,s->base.object_get_class(widget),"dnom",NULL,NULL,&status);
            if(!status){void *v=NULL;s->api.field_get_value(widget,field,&v);status=accept_reference(s,s->api.field_get_type(field),v,&gate);}
            if(!status)status=silph_managed_ascii(s->managed,template,(uint32_t)strlen(template),&key);
            SilphReadArgument argument={.kind=SILPH_READ_REFERENCE,.value.reference=key};
            if(!status)status=call_internal(s,gate.reference,NULL,"IsRolledBack",SILPH_READ_BOOL,&argument,1,&rolled_back,permit);
            const char *name=rolled_back.bits?"COMBAT_LEAGUE_DEFAULT_MASTER":"COMBAT_LEAGUE_DEFAULT_MASTER_NPC";
            if(!status)status=silph_managed_ascii(s->managed,name,(uint32_t)strlen(name),&league.reference);
            int freed=release_rpc_ref(s,&key),other=release_rpc_ref(s,&gate.reference);if(!status)status=freed?freed:other;
        }
        SilphReadArgument args[]={{.kind=SILPH_READ_REFERENCE,.value.reference=league.reference},{.kind=SILPH_READ_BOOL,.value.bits=1}};
        if(!status)status=call_internal(s,service.reference,NULL,"ConstructPartyForLeague",SILPH_READ_REFERENCE,args,2,&party,permit);
        if(!status)status=silph_managed_target(s->managed,party.reference,&object);
        if(!status&&!research_type(s,object,"Niantic.Holoholo.Battle","BattleParty"))status=SILPH_MANAGED_CONTRACT;
        SilphReadArgument selected[]={{.kind=SILPH_READ_REFERENCE,.value.reference=party.reference},{.kind=SILPH_READ_REFERENCE,.value.reference=league.reference}};
        if(!status)status=call_internal(s,page.reference,NULL,"SetBattleParty",SILPH_READ_VOID,selected,2,out,permit);
        SilphReadArgument flag={.kind=SILPH_READ_BOOL};if(!status)status=call_internal(s,page.reference,NULL,"RefreshPickers",SILPH_READ_VOID,&flag,1,out,permit);
        if(!status)__android_log_print(ANDROID_LOG_INFO,"SilphRocket","recommended team regenerated");
        SilphManagedRef *refs[]={&party.reference,&page.reference,&scroll.reference,&league.reference,&service.reference};for(unsigned i=0;i<5;i++){int freed=release_rpc_ref(s,refs[i]);if(!status)status=freed;}
        return leave_read(s,status);
    }
    if(operation>=RK_SHIELD&&operation<=RK_INCIDENT_CLOSE){
        const char *spaces[]={"Niantic.Holoholo.Combat.InputChallenge","Niantic.Holoholo.Combat.NpcTrainers","Niantic.Holoholo.Combat.PokemonPartyState","Niantic.Holoholo.Combat.Resolution","Niantic.Holoholo.Incident"};
        const char *names[]={"ShieldModalGuiController","NpcTrainerInterstitialGui","CombatPokemonPartyWidget","CombatResolveGuiController","IncidentCombatResolveGuiController"};
        const char *methods[]={"ShieldUiClicked","PressCancel","Cancel","CloseButtonPressed","Close"};unsigned index=operation-RK_SHIELD;
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,spaces[index],names[index])))status=SILPH_MANAGED_CONTRACT;
        SilphReadValue button={0},enabled={0};
        if(!status&&operation==RK_SHIELD){
            const void *f=find_field(s,s->base.object_get_class(object),"shieldButton",NULL,NULL,&status);
            if(!status){void *v=NULL;s->api.field_get_value(object,f,&v);status=accept_reference(s,s->api.field_get_type(f),v,&button);}
            if(!status)status=call_internal(s,button.reference,NULL,"get_interactable",SILPH_READ_BOOL,NULL,0,&enabled,permit);
            if(!status&&!enabled.bits)status=SILPH_MANAGED_NOT_READY;
        }
        if(!status)status=call_internal(s,receiver,NULL,methods[index],SILPH_READ_VOID,NULL,0,out,permit);
        int freed=release_rpc_ref(s,&button.reference);return leave_read(s,status?status:freed);
    }
    if(operation==RK_CAPTURE_INFO||operation==RK_CAPTURE_RECORD){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        bool record=operation==RK_CAPTURE_RECORD;
        if(!status&&(count!=(record?2u:1u)||args[0].kind!=SILPH_READ_U64||!args[0].value.bits||!research_type(s,object,"Niantic.Holoholo.UI.Pokemon",record?"PokemonSizeRecordBreakGuiController":"PokemonInfoGuiController")))status=SILPH_MANAGED_CONTRACT;
        SilphManagedRef source=receiver;if(!status&&record){if(args[1].kind!=SILPH_READ_REFERENCE||!args[1].value.reference.serial)status=SILPH_MANAGED_ARGUMENT;else source=args[1].value.reference;}
        if(!status&&record)status=silph_managed_target(s->managed,source,&object);
        if(!status&&record&&!research_type(s,object,"Niantic.Holoholo.UI.Pokemon","PokemonInfoGuiController"))status=SILPH_MANAGED_CONTRACT;
        if(!status&&record){const void *state=find_field(s,s->base.object_get_class(object),"currentState",NULL,NULL,&status);int32_t active=0;if(!status&&!matches_kind(s,s->api.field_get_type(state),SILPH_READ_ENUM32))status=SILPH_MANAGED_CONTRACT;if(!status){s->api.field_get_value(object,state,&active);if(active!=2)status=SILPH_MANAGED_NOT_READY;}}
        SilphReadValue pokemon={0},data={0};uint64_t id=0;uint8_t post=0;
        if(!status)status=call_internal(s,source,NULL,"get_CurrentPokemon",SILPH_READ_REFERENCE,NULL,0,&pokemon,permit);
        if(!status&&!pokemon.reference.serial)status=SILPH_MANAGED_NOT_READY;
        const void *field=status?NULL:find_field(s,s->base.object_get_class(object),"egbx",NULL,NULL,&status);
        if(!status){void *v=NULL;s->api.field_get_value(object,field,&v);status=accept_reference(s,s->api.field_get_type(field),v,&data);}
        if(!status&&!data.reference.serial)status=SILPH_MANAGED_NOT_READY;
        if(!status)status=silph_managed_target(s->managed,pokemon.reference,&object);
        field=status?NULL:find_field(s,s->base.object_get_class(object),"id_",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(field),SILPH_READ_U64))status=SILPH_MANAGED_CONTRACT;
        if(!status)s->api.field_get_value(object,field,&id);
        if(!status)status=silph_managed_target(s->managed,data.reference,&object);
        if(!status&&!research_type(s,object,"Niantic.Holoholo.UI.Pokemon","PokemonInfoData"))status=SILPH_MANAGED_CONTRACT;
        field=status?NULL:find_field(s,s->base.object_get_class(object),"IsPostEncounter",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(field),SILPH_READ_BOOL))status=SILPH_MANAGED_CONTRACT;
        if(!status)s->api.field_get_value(object,field,&post);
        if(!status&&(!post||id!=args[0].value.bits))status=SILPH_MANAGED_NOT_READY;
        if(!status)status=call_internal(s,receiver,NULL,record?"Dismiss":"DismissPokemonInfoPanel",SILPH_READ_VOID,NULL,0,out,permit);
        int a=release_rpc_ref(s,&data.reference),b=release_rpc_ref(s,&pokemon.reference);return leave_read(s,status?status:a?a:b);
    }
    if(operation==RK_MAP_REFRESH){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,"Niantic.Holoholo.Map","MapContentHandler")))status=SILPH_MANAGED_CONTRACT;
        SilphReadArgument argument={.kind=SILPH_READ_BOOL,.value.bits=1};
        if(!status)status=call_internal(s,receiver,NULL,"set_ForceImmediateGetObjects",SILPH_READ_VOID,&argument,1,out,permit);return leave_read(s,status);
    }
    if(operation==RK_PARTY_REFRESH){
        const char *step="widget";
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count!=1||args[0].kind!=SILPH_READ_REFERENCE||!args[0].value.reference.serial||!research_type(s,object,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget")))status=SILPH_MANAGED_CONTRACT;
        SilphReadValue scroll={0},page={0},party={0},refreshed={0};
        const void *f=status?NULL:find_field(s,s->base.object_get_class(object),"dynoScrollRect",NULL,NULL,&status);
        if(!status){void *v=NULL;s->api.field_get_value(object,f,&v);status=accept_reference(s,s->api.field_get_type(f),v,&scroll);}
        if(!status){step="page";status=call_internal(s,scroll.reference,NULL,"CurrentPageGui",SILPH_READ_REFERENCE,NULL,0,&page,permit);}
        if(!status){step="party";status=call_internal(s,page.reference,NULL,"get_BattleParty",SILPH_READ_REFERENCE,NULL,0,&party,permit);}
        if(!status){step="refresh";status=call_internal(s,party.reference,NULL,"Refresh",SILPH_READ_REFERENCE,args,1,&refreshed,permit);}
        if(!status){
            step="set_party";void *panel=NULL,*team=NULL;status=silph_managed_target(s->managed,page.reference,&panel);
            if(!status)status=silph_managed_target(s->managed,refreshed.reference,&team);
            const void *method=status?NULL:research_method(s,s->base.object_get_class(panel),"SetBattleParty",2,false);
            if(!status&&(!method||!research_type(s,panel,"Niantic.Holoholo.Gym","CombatPrepPartyPanel")||!research_type(s,team,"Niantic.Holoholo.Battle","BattleParty")
                ||!matches_kind(s,s->base.method_get_return_type(method),SILPH_READ_VOID)||!matches_kind(s,s->api.method_get_param(method,0),SILPH_READ_REFERENCE)
                ||s->base.type_get_type(s->api.method_get_param(method,1))!=14||!s->api.class_is_assignable_from(s->api.class_from_type(s->api.method_get_param(method,0)),s->base.object_get_class(team))))status=SILPH_MANAGED_CONTRACT;
            if(!status){void *values[]={team,NULL},*exception=NULL;s->base.runtime_invoke(method,panel,values,&exception);if(exception)status=SILPH_MANAGED_EXCEPTION;}
        }
        SilphReadArgument flag={.kind=SILPH_READ_BOOL};if(!status){step="pickers";status=call_internal(s,page.reference,NULL,"RefreshPickers",SILPH_READ_VOID,&flag,1,out,permit);}
        if(status)__android_log_print(ANDROID_LOG_ERROR,"SilphRocket","team refresh step=%s error=%d",step,status);
        else __android_log_print(ANDROID_LOG_INFO,"SilphRocket","team selection refreshed from inventory");
        SilphManagedRef *refs[]={&refreshed.reference,&party.reference,&page.reference,&scroll.reference};for(unsigned i=0;i<4;i++){int freed=release_rpc_ref(s,refs[i]);if(!status)status=freed;}
        return leave_read(s,status);
    }
    if(operation==RK_LAYER){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&!research_type(s,object,"Niantic.Holoholo.NUI","GuiLayerService"))status=SILPH_MANAGED_CONTRACT;
        if(!status&&(count!=1||args[0].kind!=SILPH_READ_ENUM32||args[0].value.bits>1100))status=SILPH_MANAGED_ARGUMENT;
        if(!status)status=call_internal(s,receiver,NULL,"GetRegisteredGuisForLayer",SILPH_READ_REFERENCE,args,count,out,permit);
        return leave_read(s,status);
    }
    if(operation==RK_PARTY){
        void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
        if(!status&&(count||!research_type(s,object,"Niantic.Holoholo.Combat.Preparation","CombatPreparationRootGuiController")))status=SILPH_MANAGED_CONTRACT;
        SilphReadValue root={0};SilphManagedRef reflection={0};
        if(!status)status=call_internal(s,receiver,NULL,"get_RootTransform",SILPH_READ_REFERENCE,NULL,0,&root,permit);
        void *klass=lookup_class(s,"holo-game.dll","Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget");
        void *type=klass?s->api.type_get_object(s->base.class_get_type(klass)):NULL;
        if(!status)status=type?silph_managed_retain(s->managed,type,false,&reflection):SILPH_MANAGED_CONTRACT;
        SilphReadArgument argument={.kind=SILPH_READ_REFERENCE,.value.reference=reflection};
        if(!status)status=call_internal(s,root.reference,NULL,"GetComponentInChildren",SILPH_READ_REFERENCE,&argument,1,out,permit);
        int a=release_rpc_ref(s,&root.reference),b=release_rpc_ref(s,&reflection);return leave_read(s,status?status:a?a:b);
    }
    const char *spaces[]={"Niantic.Holoholo.NUI","Niantic.Holoholo.Pokestop","Niantic.Holoholo.Combat.NpcTrainers","Niantic.Holoholo.Combat.PokemonPartyState","Niantic.Holoholo.Incident","Niantic.Holoholo.UI","Niantic.Holoholo.Incident","Niantic.Holoholo.Incident","Niantic.Holoholo.Encounter","Niantic.Holoholo.Encounter","Niantic.Holoholo.Incident","Niantic.Holoholo.Pokestop"};
    const char *names[]={"GuiLayerService","MapPokestop","NpcTrainerInterstitialGui","CombatPokemonPartyWidget","IncidentCombatResolveGuiController","ItemFragmentAssemblyService","IncidentService","IncidentService","EncounterState","EncounterState","IncidentInvasionDialogGui","PokestopSpinnerInteractionGuiController"};
    const char *methods[]={"TopGui","OnTap","PressChallenge","SubmitButtonPressed","DoneButtonClicked","DismissAllOpenModals","StartIncidentEncounter","StopCurrentIncident","EncounterStateComplete","CaptureCompleted","ClickedToContinue","ClickExit"};
    void *object=NULL;status=silph_managed_target(s->managed,receiver,&object);
    if(!status&&!research_type(s,object,spaces[operation-1],names[operation-1]))status=SILPH_MANAGED_CONTRACT;
    if(!status&&operation==RK_TAP){
        if(count)status=SILPH_MANAGED_ARGUMENT;
        const void *method=status?NULL:research_method(s,s->base.object_get_class(object),"OnTap",2,false);
        if(!status&&(!method||!matches_kind(s,s->base.method_get_return_type(method),SILPH_READ_VOID)
            ||!matches_kind(s,s->api.method_get_param(method,0),SILPH_READ_REFERENCE)||!matches_kind(s,s->api.method_get_param(method,1),SILPH_READ_REFERENCE)))status=SILPH_MANAGED_CONTRACT;
        if(!status){void *values[2]={0},*exception=NULL;s->base.runtime_invoke(method,object,values,&exception);if(exception)status=SILPH_MANAGED_EXCEPTION;}
    }else if(!status){
        if(operation==RK_ENCOUNTER_FINISH&&(count!=1||args[0].kind!=SILPH_READ_ENUM32||args[0].value.bits>3))status=SILPH_MANAGED_ARGUMENT;
        if(!status)status=call_internal(s,receiver,NULL,methods[operation-1],operation==RK_TOP?SILPH_READ_REFERENCE:SILPH_READ_VOID,args,count,out,permit);
    }
    return leave_read(s,status);
}
int silph_rocket_class(void *memory,SilphManagedRef ref,char *output,uint32_t capacity){
    if(!output||capacity<2||capacity>256)return SILPH_MANAGED_ARGUMENT;output[0]=0;
    Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;
    status=silph_managed_target(s->managed,ref,&object);const char *name=status?NULL:s->api.class_get_name(s->base.object_get_class(object));
    if(!status&&(!name||strnlen(name,capacity)>=capacity))status=SILPH_MANAGED_CONTRACT;
    if(!status)memcpy(output,name,strlen(name)+1);return leave_read(s,status);
}
int silph_rocket_medicine(void *memory,const SilphSchedulePermit *permit,SilphManagedRef bag,uint64_t id,uint32_t item,SilphManagedRef *out){
    if(!out||!id||!((item>=101&&item<=104)||item==201||item==202)||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_ARGUMENT;
    *out=(SilphManagedRef){0};Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;
    status=silph_managed_target(s->managed,bag,&object);if(!status&&!research_type(s,object,"Niantic.Holoholo.Internal","ItemBagImpl"))status=SILPH_MANAGED_CONTRACT;
    SilphReadArgument args[]={{.kind=SILPH_READ_ENUM32,.value.bits=item},{.kind=SILPH_READ_U64,.value.bits=id}};SilphReadValue value={0};
    if(!status)status=call_internal(s,bag,NULL,"UseItemOnPokemon",SILPH_READ_REFERENCE,args,2,&value,permit);
    if(!status){*out=value.reference;value.reference=(SilphManagedRef){0};}int freed=release_rpc_ref(s,&value.reference);return leave_read(s,status?status:freed);
}
int silph_rocket_prepared_team(void *memory,SilphManagedRef widget,uint64_t out[3],bool *usable){
    if(!out||!usable)return SILPH_MANAGED_ARGUMENT;*usable=true;memset(out,0,3*sizeof(*out));Read *s=memory;int status=enter_read(s);if(status)return status;
    void *object=NULL;status=silph_managed_target(s->managed,widget,&object);
    if(!status&&!research_type(s,object,"Niantic.Holoholo.Combat.PokemonPartyState","CombatPokemonPartyWidget"))status=SILPH_MANAGED_CONTRACT;
    SilphManagedRef scroll={0};SilphReadValue page={0},party={0},array={0},length={0},pokemon={0};
    const void *field=status?NULL:find_field(s,s->base.object_get_class(object),"dynoScrollRect",NULL,NULL,&status);
    if(!status){void *v=NULL;s->api.field_get_value(object,field,&v);status=accept_reference(s,s->api.field_get_type(field),v,&page);scroll=page.reference;clear_value(&page);}
    if(!status)status=call_internal(s,scroll,NULL,"CurrentPageGui",SILPH_READ_REFERENCE,NULL,0,&page,NULL);
    if(!status&&!page.reference.serial)status=SILPH_MANAGED_NOT_READY;
    if(!status)status=call_internal(s,page.reference,NULL,"get_BattleParty",SILPH_READ_REFERENCE,NULL,0,&party,NULL);
    if(!status)status=silph_managed_target(s->managed,party.reference,&object);
    field=status?NULL:find_field(s,s->base.object_get_class(object),"TeamProtos",NULL,NULL,&status);
    if(!status){void *v=NULL;s->api.field_get_value(object,field,&v);status=accept_reference(s,s->api.field_get_type(field),v,&array);}
    if(!status)status=call_internal(s,array.reference,NULL,"get_Length",SILPH_READ_I32,NULL,0,&length,NULL);
    if(!status&&length.bits!=3)status=SILPH_MANAGED_CONTRACT;
    for(unsigned i=0;!status&&i<3;i++){
        SilphReadArgument argument={.kind=SILPH_READ_I32,.value.bits=i};status=call_internal(s,array.reference,NULL,"GetValue",SILPH_READ_REFERENCE,&argument,1,&pokemon,NULL);
        if(!status)status=silph_managed_target(s->managed,pokemon.reference,&object);
        if(!status&&!research_type(s,object,"Holoholo.Rpc","PokemonProto"))status=SILPH_MANAGED_CONTRACT;
        field=status?NULL:find_field(s,s->base.object_get_class(object),"id_",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(field),SILPH_READ_U64))status=SILPH_MANAGED_CONTRACT;
        if(!status){s->api.field_get_value(object,field,&out[i]);if(!out[i])status=SILPH_MANAGED_CONTRACT;for(unsigned j=0;j<i;j++)if(out[j]==out[i])status=SILPH_MANAGED_CONTRACT;}
        uint32_t hp=0,max=0;field=status?NULL:find_field(s,s->base.object_get_class(object),"stamina_",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(field),SILPH_READ_I32))status=SILPH_MANAGED_CONTRACT;
        if(!status)s->api.field_get_value(object,field,&hp);field=status?NULL:find_field(s,s->base.object_get_class(object),"maxStamina_",NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(field),SILPH_READ_I32))status=SILPH_MANAGED_CONTRACT;
        if(!status){s->api.field_get_value(object,field,&max);if(!hp||!max||hp!=max)*usable=false;}
        int d=release_rpc_ref(s,&pokemon.reference);if(!status)status=d;
    }
    SilphManagedRef *refs[]={&pokemon.reference,&array.reference,&party.reference,&page.reference,&scroll};for(unsigned i=0;i<5;i++){int d=release_rpc_ref(s,refs[i]);if(!status)status=d;}
    return leave_read(s,status);
}
int silph_rocket_map_settings(void *memory,const SilphSchedulePermit *permit,SilphManagedRef ref,float saved[3],bool restore){
    if(!saved||(!restore&&!silph_schedule_permitted(permit,true)))return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;void *object=NULL;
    status=silph_managed_target(s->managed,ref,&object);if(!status&&!research_type(s,object,"Holoholo.Rpc","MapSettingsProto"))status=SILPH_MANAGED_CONTRACT;
    const char *names[]={"GetMapObjectsMinRefreshSeconds","GetMapObjectsMaxRefreshSeconds","GetMapObjectsMinDistanceMeters"};
    const float applied[]={5,10,8};float current[3]={0};const void *fields[3]={0};
    for(unsigned i=0;!status&&i<3;i++){
        fields[i]=find_field(s,s->base.object_get_class(object),names[i],NULL,NULL,&status);
        if(!status&&!matches_kind(s,s->api.field_get_type(fields[i]),SILPH_READ_F32))status=SILPH_MANAGED_CONTRACT;
        if(!status){s->api.field_get_value(object,fields[i],&current[i]);if(!isfinite(current[i])||current[i]<0||current[i]>3600||(restore&&(!isfinite(saved[i])||saved[i]<0||saved[i]>3600)))status=SILPH_MANAGED_CONTRACT;}
    }
    if(!status)for(unsigned i=0;i<3;i++){if(!restore)saved[i]=current[i];if(!restore||current[i]==applied[i]){float value=restore?saved[i]:applied[i];s->field_set_value(object,fields[i],&value);}}
    return leave_read(s,status);
}
static void *rocket_nested(Read *s,void *parent,const char *wanted){
    if(!s->research_nested)return NULL;void *iterator=NULL,*found=NULL;
    for(unsigned i=0;parent&&i<256;i++){void *type=s->research_nested(parent,&iterator);if(!type)break;const char *name=s->api.class_get_name(type);
        if(name&&!strcmp(name,wanted)){if(found)return NULL;found=type;}}
    return found;
}
int silph_rocket_attack(void *memory,const SilphSchedulePermit *permit,SilphManagedRef director,uint32_t attack,float score,uint64_t outgoing,uint64_t incoming,SilphManagedRef *out){
    if(!out)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));
    if(!silph_schedule_permitted(permit,true)||!isfinite(score)||score<0||score>1||attack<1||attack>7)return SILPH_MANAGED_STATE;
    if(attack==7&&(!outgoing||!incoming||outgoing==incoming))return SILPH_MANAGED_ARGUMENT;
    if(attack>4&&attack!=7)return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;void *receiver=NULL;
    status=silph_managed_target(s->managed,director,&receiver);
    if(!status&&!research_type(s,receiver,"Niantic.Holoholo.VNext","BattleDirector"))status=SILPH_MANAGED_CONTRACT;
    SilphManagedRef event={0},payload={0};SilphReadValue value={0};
    if(!status)status=fresh_proto(s,"BattleEventProto",&event);
    void *parent=lookup_class(s,"holo-protos.dll","Holoholo.Rpc","BattleEventProto");
    void *nested=rocket_nested(s,rocket_nested(s,parent,"Types"),attack==7?"SwapPokemon":"Attack");
    if(!status&&!nested)status=SILPH_MANAGED_CONTRACT;
    if(!status){void *object=s->object_new(nested);status=object?silph_managed_retain(s->managed,object,false,&payload):SILPH_MANAGED_CAPACITY;}
    if(!status)status=call_internal(s,payload,NULL,".ctor",SILPH_READ_VOID,NULL,0,&value,permit);
    if(!status&&attack==7){status=fresh_scalar(s,payload,"OutgoingPokemonId",SILPH_READ_U64,outgoing);if(!status)status=fresh_scalar(s,payload,"IncomingPokemonId",SILPH_READ_U64,incoming);}
    if(!status&&attack!=7){uint32_t bits;memcpy(&bits,&score,4);status=fresh_scalar(s,payload,"AttackType",SILPH_READ_ENUM32,attack);if(!status)status=fresh_scalar(s,payload,"Score",SILPH_READ_F32,bits);}
    SilphReadArgument argument={.kind=SILPH_READ_REFERENCE,.value.reference=payload};
    if(!status)status=call_internal(s,event,NULL,attack==7?"set_SwapPokemon":"set_Attack",SILPH_READ_VOID,&argument,1,&value,permit);
    if(!status)status=fresh_scalar(s,event,"Type",SILPH_READ_ENUM32,attack==7?7:4);
    SilphReadArgument args[]={{.kind=SILPH_READ_REFERENCE,.value.reference=event},{.kind=SILPH_READ_BOOL,.value.bits=1}};
    if(!status)status=call_internal(s,director,NULL,"TrySubmitPlayerAction",SILPH_READ_REFERENCE,args,2,&value,permit);
    if(!status){*out=value.reference;value.reference=(SilphManagedRef){0};}
    int a=release_rpc_ref(s,&payload),b=release_rpc_ref(s,&event),c=release_rpc_ref(s,&value.reference);
    return leave_read(s,status?status:a?a:b?b:c);
}
