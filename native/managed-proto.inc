static bool coordinate(double a,double b){return isfinite(a)&&isfinite(b)&&fabs(a)<=90&&fabs(b)<=180;}
static bool berry_id(int32_t item){return item==701||item==703||item==705||item==706||item==708;}
static bool plain_id(const char *text,size_t maximum){
    size_t n=strnlen(text,maximum+1);if(!n||n>maximum)return false;
    for(size_t i=0;i<n;i++){unsigned char c=text[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c==':'||c=='-'))return false;}return true;
}
static bool encounter_token(const char *text){
    size_t n=strnlen(text,17);if(!n||n>16)return false;uint64_t bits=0;
    for(size_t i=0;i<16;i++){unsigned c=i<n?(unsigned char)text[i]:'0';unsigned v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:16;if(v>15)return false;bits=(bits<<4)|v;}
    return bits&&(bits>>61)<=5;
}
int silph_proto_validate(uint32_t operation,const SilphRequestInput *input){
    if(!input||input->abi!=1||input->reserved||operation<1||operation>=SILPH_RPC_GIFT_OPEN)return SILPH_MANAGED_ARGUMENT;
    if(operation==SILPH_RPC_BUDDY_CHECK||operation==SILPH_RPC_BUDDY_REMOVE)return input->encounter||input->item||input->count||input->fort[0]||input->location[0]?SILPH_MANAGED_ARGUMENT:0;
    if(operation==SILPH_RPC_KECLEON_GET)return plain_id(input->fort,160)&&coordinate(input->latitude,input->longitude)?0:SILPH_MANAGED_ARGUMENT;
    if(operation==SILPH_RPC_SPIN)return plain_id(input->fort,160)&&coordinate(input->latitude,input->longitude)
        &&coordinate(input->player_latitude,input->player_longitude)?0:SILPH_MANAGED_ARGUMENT;
    if(operation==SILPH_RPC_DISCARD)return input->item>0&&input->item!=4&&input->count>0&&input->count<=50?0:SILPH_MANAGED_ARGUMENT;
    if(!input->encounter||!encounter_token(input->location))return SILPH_MANAGED_ARGUMENT;
    if(operation==SILPH_RPC_KECLEON_OPEN)return 0;
    if(operation==SILPH_RPC_KECLEON_BERRY)return berry_id(input->item)?0:SILPH_MANAGED_ARGUMENT;
    return input->item>=1&&input->item<=3&&input->quality>=1&&input->quality<=4&&input->curve<=1?0:SILPH_MANAGED_ARGUMENT;
}
int silph_proto_enable(void *memory,SilphManagedResolver resolve,void *context){
    if(!resolve)return SILPH_MANAGED_ARGUMENT;
    Read *s=memory;int status=enter_read(s);if(status)return status;
    void *create=resolve("il2cpp_object_new",context),*scalar=resolve("il2cpp_field_set_value",context),*reference=resolve("il2cpp_field_set_value_object",context);
    if(!create||!scalar||!reference)status=SILPH_MANAGED_ABI_MISMATCH;
    if(!status){memcpy(&s->object_new,&create,sizeof(create));memcpy(&s->field_set_value,&scalar,sizeof(scalar));memcpy(&s->field_set_value_object,&reference,sizeof(reference));}
    return leave_read(s,status);
}
static int fresh_proto(Read *s,const char *name,SilphManagedRef *output){
    void *klass=lookup_class(s,"holo-protos.dll","Holoholo.Rpc",name);if(!klass||s->api.class_is_valuetype(klass))return SILPH_MANAGED_CONTRACT;
    void *object=s->object_new(klass);if(!object)return SILPH_MANAGED_CAPACITY;
    int status=silph_managed_retain(s->managed,object,false,output);SilphReadValue value={0};
    if(!status)status=call_internal(s,*output,NULL,".ctor",SILPH_READ_VOID,NULL,0,&value,NULL);return status;
}
static int fresh_scalar(Read *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t bits){
    void *object=NULL;int status=silph_managed_target(s->managed,ref,&object);
    const void *field=status?NULL:find_field(s,s->base.object_get_class(object),name,NULL,NULL,&status);
    if(!status&&!matches_kind(s,s->api.field_get_type(field),kind))status=SILPH_MANAGED_CONTRACT;
    if(!status)s->field_set_value(object,field,&bits);return status;
}
static int fresh_reference(Read *s,SilphManagedRef ref,const char *name,SilphManagedRef value){
    void *object=NULL,*target=NULL;int status=silph_managed_target(s->managed,ref,&object);
    if(!status)status=silph_managed_target(s->managed,value,&target);
    const void *field=status?NULL:find_field(s,s->base.object_get_class(object),name,NULL,NULL,&status);
    const void *type=field?s->api.field_get_type(field):NULL;
    if(!status&&(!matches_kind(s,type,SILPH_READ_REFERENCE)||!s->api.class_is_assignable_from(s->api.class_from_type(type),s->base.object_get_class(target))))status=SILPH_MANAGED_CONTRACT;
    if(!status)s->field_set_value_object(object,field,target);return status;
}
static int fresh_text(Read *s,SilphManagedRef ref,const char *name,const char *text){
    size_t n=strnlen(text,161);if(n>160)return SILPH_MANAGED_ARGUMENT;
    void *object=s->base.string_new_len(text,(uint32_t)n);if(!object)return SILPH_MANAGED_CAPACITY;
    SilphManagedRef held={0};int status=silph_managed_retain(s->managed,object,false,&held);
    if(!status)status=fresh_reference(s,ref,name,held);int freed=release_rpc_ref(s,&held);return status?status:freed;
}
static int fresh_double(Read *s,SilphManagedRef ref,const char *name,double value){uint64_t bits=0;memcpy(&bits,&value,8);return fresh_scalar(s,ref,name,SILPH_READ_F64,bits);}
static int geometry_token(Read *s,double lat,double lon,char output[17]){
    if(!coordinate(lat,lon))return SILPH_MANAGED_ARGUMENT;output[0]=0;
    void *ll=lookup_class(s,"Zeppelin.dll","Google.Common.Geometry","S2LatLng"),*cell=lookup_class(s,"Zeppelin.dll","Google.Common.Geometry","S2CellId");
    if(!ll||!cell||!s->api.class_is_valuetype(ll)||!s->api.class_is_valuetype(cell))return SILPH_MANAGED_CONTRACT;
    SilphReadArgument args[2]={{.kind=SILPH_READ_F64},{.kind=SILPH_READ_F64}};memcpy(&args[0].value.bits,&lat,8);memcpy(&args[1].value.bits,&lon,8);
    SilphReadValue a={0},b={0},c={0},token={0};
    int status=call_internal(s,(SilphManagedRef){0},ll,"FromDegrees",SILPH_READ_BOXED,args,2,&a,NULL);
    SilphReadArgument arg={.kind=SILPH_READ_BOXED,.value.reference=a.reference};
    if(!status)status=call_internal(s,(SilphManagedRef){0},cell,"FromLatLng",SILPH_READ_BOXED,&arg,1,&b,NULL);
    arg=(SilphReadArgument){.kind=SILPH_READ_I32,.value.bits=20};
    if(!status)status=call_internal(s,b.reference,NULL,"ParentForLevel",SILPH_READ_BOXED,&arg,1,&c,NULL);
    if(!status)status=call_internal(s,c.reference,NULL,"ToToken",SILPH_READ_REFERENCE,NULL,0,&token,NULL);
    if(!status){void *text=NULL;status=silph_managed_target(s->managed,token.reference,&text);
        if(!status){int32_t n=s->base.string_length(text);const uint16_t *chars=s->api.string_chars(text);
            if(n<1||n>16||!chars)status=SILPH_MANAGED_CONTRACT;
            else{for(int32_t i=0;i<n;i++){if(chars[i]>127){status=SILPH_MANAGED_CONTRACT;break;}output[i]=(char)chars[i];}output[n]=0;}
        }
        if(!status&&!encounter_token(output))status=SILPH_MANAGED_CONTRACT;
        if(!status){uint64_t bits=0;size_t n=strlen(output);for(size_t i=0;i<16;i++){unsigned v=i<n?(unsigned char)output[i]:'0';bits=(bits<<4)|(v<='9'?v-'0':v-'a'+10);}if((bits&(~bits+1))!=(UINT64_C(1)<<20))status=SILPH_MANAGED_CONTRACT;}
    }
    SilphManagedRef *refs[]={&a.reference,&b.reference,&c.reference,&token.reference};for(unsigned i=0;i<4;i++){int freed=release_rpc_ref(s,refs[i]);if(!status)status=freed;}
    if(status)output[0]=0;return status;
}
int silph_proto_token(void *memory,double lat,double lon,char *output,uint32_t capacity){
    if(!output||capacity<17)return SILPH_MANAGED_ARGUMENT;output[0]=0;
    Read *s=memory;int status=enter_read(s);if(status)return status;status=geometry_token(s,lat,lon,output);return leave_read(s,status);
}
int silph_proto_request(void *memory,uint32_t operation,const SilphRequestInput *input,SilphManagedRef *output){
    if(!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));int status=silph_proto_validate(operation,input);if(status)return status;
    Read *s=memory;status=enter_read(s);if(status)return status;
    if(!s->object_new||!s->field_set_value||!s->field_set_value_object)return leave_read(s,SILPH_MANAGED_STATE);
    status=fresh_proto(s,rpc_schemas[operation-1].request,output);char token[17]={0};SilphManagedRef ar={0};
    if(!status&&operation==SILPH_RPC_BUDDY_CHECK)status=fresh_scalar(s,*output,"BuddyHomeWidgetActive",SILPH_READ_BOOL,0);
    if(!status&&operation==SILPH_RPC_BUDDY_REMOVE)status=fresh_scalar(s,*output,"PokemonId",SILPH_READ_U64,0);
    if(!status&&operation==SILPH_RPC_KECLEON_GET)status=geometry_token(s,input->latitude,input->longitude,token);
    if(!status&&operation==SILPH_RPC_KECLEON_GET)status=fresh_scalar(s,*output,"PokemonId",SILPH_READ_ENUM32,352);
    if(!status&&(operation==SILPH_RPC_KECLEON_GET||operation==SILPH_RPC_SPIN))status=fresh_text(s,*output,operation==SILPH_RPC_SPIN?"Id":"FortId",input->fort);
    if(!status&&operation==SILPH_RPC_KECLEON_GET)status=fresh_text(s,*output,"IncidentId","");
    if(!status&&operation==SILPH_RPC_KECLEON_GET)status=fresh_text(s,*output,"EncounterLocation",token);
    if(!status&&(operation==SILPH_RPC_KECLEON_OPEN||operation==SILPH_RPC_KECLEON_BERRY||operation==SILPH_RPC_KECLEON_CAPTURE))status=fresh_scalar(s,*output,"EncounterId",SILPH_READ_U64,input->encounter);
    if(!status&&(operation==SILPH_RPC_KECLEON_OPEN||operation==SILPH_RPC_KECLEON_BERRY||operation==SILPH_RPC_KECLEON_CAPTURE))status=fresh_text(s,*output,operation==SILPH_RPC_KECLEON_OPEN?"EncounterLocation":"SpawnPointGuid",input->location);
    if(!status&&(operation==SILPH_RPC_KECLEON_BERRY||operation==SILPH_RPC_DISCARD))status=fresh_scalar(s,*output,"Item",SILPH_READ_ENUM32,(uint32_t)input->item);
    if(!status&&operation==SILPH_RPC_DISCARD)status=fresh_scalar(s,*output,"Count",SILPH_READ_I32,(uint32_t)input->count);
    if(!status&&operation==SILPH_RPC_SPIN){
        const char *names[]={"PlayerLatDegrees","PlayerLngDegrees","FortLatDegrees","FortLngDegrees"};double values[]={input->player_latitude,input->player_longitude,input->latitude,input->longitude};
        for(unsigned i=0;!status&&i<4;i++)status=fresh_double(s,*output,names[i],values[i]);
    }
    if(!status&&operation==SILPH_RPC_KECLEON_CAPTURE){
        const double profiles[]={0,1.1,.85,.5,.05};status=fresh_scalar(s,*output,"Pokeball",SILPH_READ_I32,(uint32_t)input->item);
        if(!status)status=fresh_double(s,*output,"NormalizedReticleSize",2-profiles[input->quality]);
        if(!status)status=fresh_scalar(s,*output,"HitPokemon",SILPH_READ_BOOL,1);
        if(!status)status=fresh_double(s,*output,"SpinModifier",input->curve?1:0);
        if(!status)status=fresh_double(s,*output,"NormalizedHitPosition",1);
        if(!status)status=fresh_scalar(s,*output,"ShouldFlee",SILPH_READ_BOOL,0);
        if(!status)status=fresh_proto(s,"ARPlusEncounterValuesProto",&ar);
        if(!status)status=fresh_reference(s,*output,"ArPlusValues",ar);
    }
    int freed=release_rpc_ref(s,&ar);if(!status)status=freed;
    if(status)release_rpc_ref(s,output);return leave_read(s,status);
}
