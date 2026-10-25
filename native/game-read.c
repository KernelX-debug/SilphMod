#include "game-read.h"
#include <string.h>
#include <math.h>

typedef struct {
    void *read,*managed;
    SilphManagedApi api;
    SilphManagedRef map,container,player,pokemon_bag,item_bag,directory,directory_type;
    char account[1025];
    uint64_t samples,account_hi,account_lo;
    SilphGameSnapshot previous;
    unsigned opened,stage,snapshot_ready,compact;
} GameRead;
size_t silph_game_read_size(void) { return sizeof(GameRead); }
unsigned silph_game_read_stage(void *memory) { return memory?((GameRead *)memory)->stage:0; }
uint64_t silph_game_read_samples(void *memory){GameRead *s=memory;return s&&s->opened?s->samples:0;}
int silph_game_read_compact(void *memory){GameRead *s=memory;if(!s||!s->opened||s->samples)return SILPH_MANAGED_STATE;s->compact=1;return 0;}
static int release(GameRead *state,SilphManagedRef *ref) {
    if(!ref->serial)return 0;
    int status=silph_managed_release(state->managed,*ref);if(!status)memset(ref,0,sizeof(*ref));return status;
}
static int scalar(GameRead *s,SilphManagedRef ref,const char *name,uint32_t *output) {
    SilphReadValue value;int status=silph_read_call(s->read,ref,name,SILPH_READ_I32,NULL,0,&value);
    int32_t count=0;if(!status)memcpy(&count,&value.bits,4);
    if(!status&&(count<0||count>100000))status=SILPH_MANAGED_CONTRACT;
    if(!status)*output=(uint32_t)count;return status;
}
static int account(GameRead *state,char *output,size_t bytes) {
    SilphReadValue value;int status=silph_read_call(state->read,state->player,"get_PlayerId",SILPH_READ_REFERENCE,NULL,0,&value);
    if(!status&&!value.reference.serial)status=SILPH_MANAGED_CONTRACT;
    SilphReadValue size={0};
    if(!status)status=silph_read_call(state->read,value.reference,"get_Length",SILPH_READ_I32,NULL,0,&size);
    if(!status&&(!size.bits||size.bits>256))status=SILPH_MANAGED_CONTRACT;
    uint32_t length=0;
    if(!status)status=silph_read_utf8(state->read,value.reference,output,(uint32_t)bytes,&length);
    if(!status&&(!length||length>1024))status=SILPH_MANAGED_CONTRACT;
    if(!status)for(uint32_t i=0;i<length;i++)if((unsigned char)output[i]<=32||output[i]==127){status=SILPH_MANAGED_CONTRACT;break;}
    int freed=release(state,&value.reference);return status?status:freed;
}
static uint64_t hash_account(const char *text,uint64_t hash) {
    for(const unsigned char *p=(const void *)text;*p;p++){hash^=*p;hash*=UINT64_C(1099511628211);}return hash;
}
int silph_game_read_close(void *memory) {
    GameRead *s=memory;if(!s)return SILPH_MANAGED_ARGUMENT;
    SilphManagedRef *refs[]={&s->directory_type,&s->directory,&s->item_bag,&s->pokemon_bag,&s->player,&s->container,&s->map};int first=0;
    for(unsigned i=0;i<7;i++){int status=release(s,refs[i]);if(!first)first=status;}
    if(!first)memset(s,0,sizeof(*s));return first;
}
int silph_game_read_open(void *memory,size_t bytes,void *read,const SilphManagedApi *api,void *managed,void *map) {
    if(!memory||bytes!=sizeof(GameRead)||!read||!api||!managed||!map)return SILPH_MANAGED_ARGUMENT;
    GameRead *s=memory;if(s->opened)return SILPH_MANAGED_STATE;
    memset(s,0,bytes);s->read=read;s->managed=managed;s->api=*api;s->opened=1;
    s->stage=10;int status=silph_managed_retain(managed,map,false,&s->map);
    if(!status){s->stage=11;status=silph_read_field_type(read,s->map,"Zenject","DiContainer",&s->container);}
    if(!status){s->stage=12;status=silph_read_field_type(read,s->map,"Niantic.Holoholo","IPlayerService",&s->player);}
    if(!status){s->stage=13;status=silph_read_field_type(read,s->map,"Niantic.Holoholo","IPokemonBag",&s->pokemon_bag);}
    if(!status){s->stage=14;status=silph_read_field_type(read,s->map,"Niantic.Holoholo","IItemBag",&s->item_bag);}
    if(!status&&(!s->container.serial||!s->player.serial||!s->pokemon_bag.serial||!s->item_bag.serial))status=SILPH_MANAGED_CONTRACT;
    if(!status){s->stage=15;status=silph_read_type_object(read,"holo-game.dll","Niantic.Holoholo.Map","IMapPlaceDirectoryService",&s->directory_type);}
    if(!status) {
        SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=s->directory_type};SilphReadValue value;
        s->stage=16;status=silph_read_call(read,s->container,"TryResolve",SILPH_READ_REFERENCE,&arg,1,&value);s->directory=value.reference;
        if(!status&&!s->directory.serial)status=SILPH_MANAGED_CONTRACT;
    }
    if(!status){s->stage=17;status=account(s,s->account,sizeof(s->account));}
    if(!status) {
        s->account_hi=hash_account(s->account,UINT64_C(14695981039346656037));
        s->account_lo=hash_account(s->account,UINT64_C(7809847782465536322));
    }else {unsigned stage=s->stage;silph_game_read_close(s);s->stage=stage;}
    return status;
}
static int same_field(GameRead *s,void *map,const char *space,const char *name,SilphManagedRef previous) {
    SilphManagedRef fresh={0};int status=silph_read_field_type(s->read,s->map,space,name,&fresh);
    void *a=NULL,*b=NULL,*held_map=NULL;
    if(!status)status=silph_managed_target(s->managed,s->map,&held_map);
    if(!status&&held_map!=map)status=SILPH_MANAGED_STALE;
    if(!status)status=silph_managed_target(s->managed,fresh,&a);
    if(!status)status=silph_managed_target(s->managed,previous,&b);
    if(!status&&a!=b)status=SILPH_MANAGED_STALE;
    int freed=release(s,&fresh);return status?status:freed;
}
static int candidates(GameRead *s,SilphManagedRef dictionary,SilphGameSnapshot *snapshot) {
    SilphReadValue collection={0},iterator={0},current={0},value={0};
    s->stage=41;int status=silph_read_call(s->read,dictionary,"get_Values",SILPH_READ_REFERENCE,NULL,0,&collection);
    if(!status){s->stage=42;status=silph_read_call(s->read,collection.reference,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&iterator);}
    for(unsigned i=0;!status&&i<8;i++) {
        s->stage=43;status=silph_read_call(s->read,iterator.reference,"MoveNext",SILPH_READ_BOOL,NULL,0,&value);
        if(status||!value.bits)break;
        s->stage=44;status=silph_read_call(s->read,iterator.reference,"get_Current",SILPH_READ_REFERENCE,NULL,0,&current);
        if(!status){s->stage=45;status=silph_read_call(s->read,current.reference,"get_EncounterId",SILPH_READ_U64,NULL,0,&value);}
        uint64_t encounter=value.bits;
        if(!status&&!encounter)status=SILPH_MANAGED_CONTRACT;
        uint32_t species=0;
        if(!status){s->stage=46;status=scalar(s,current.reference,"get_PokemonId",&species);}
        if(!status&&!species)status=SILPH_MANAGED_CONTRACT;
        if(!status) {
            if(!snapshot->candidates_read){snapshot->first_encounter=encounter;snapshot->first_species=species;}
            snapshot->candidates_read++;
        }
        int freed=release(s,&current.reference);if(!status)status=freed;
    }
    int disposed=0;
    if(iterator.reference.serial){if(!status)s->stage=47;disposed=silph_read_call(s->read,iterator.reference,"Dispose",SILPH_READ_VOID,NULL,0,&value);}
    int a=release(s,&current.reference),b=release(s,&iterator.reference),c=release(s,&collection.reference);
    return status?status:disposed?disposed:a?a:b?b:c;
}
int silph_game_read_poll(void *memory,void *map,bool inventory,SilphGameSnapshot *output) {
    GameRead *s=memory;if(!s||!s->opened||!map||!output)return SILPH_MANAGED_ARGUMENT;
    memset(output,0,sizeof(*output));
    s->stage=20;int status=same_field(s,map,"Zenject","DiContainer",s->container);
    if(!status)status=same_field(s,map,"Niantic.Holoholo","IPlayerService",s->player);
    if(!status)status=same_field(s,map,"Niantic.Holoholo","IPokemonBag",s->pokemon_bag);
    if(!status)status=same_field(s,map,"Niantic.Holoholo","IItemBag",s->item_bag);
    if(!status) {
        SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=s->directory_type};SilphReadValue fresh;
        status=silph_read_call(s->read,s->container,"TryResolve",SILPH_READ_REFERENCE,&arg,1,&fresh);
        void *a=NULL,*b=NULL;
        if(!status)status=silph_managed_target(s->managed,fresh.reference,&a);
        if(!status)status=silph_managed_target(s->managed,s->directory,&b);
        if(!status&&a!=b)status=SILPH_MANAGED_STALE;
        int freed=release(s,&fresh.reference);if(!status)status=freed;
    }
    char id[sizeof(s->account)]={0};if(!status)status=account(s,id,sizeof(id));
    if(!status&&strcmp(id,s->account))status=SILPH_MANAGED_STALE;
    if(!status&&!inventory&&s->snapshot_ready){*output=s->previous;output->samples=++s->samples;return 0;}
    if(!status)status=scalar(s,s->pokemon_bag,"get_Count",&output->pokemon_count);
    if(!status)status=scalar(s,s->player,"get_MaxPokemon",&output->max_pokemon);
    if(!status)status=scalar(s,s->item_bag,"GetTotalItems",&output->item_count);
    if(!status)status=scalar(s,s->player,"get_MaxItems",&output->max_items);
    const int32_t items[]={1,2,3,701,703,705,706,708};
    for(unsigned i=0;!status&&i<8;i++) {
        SilphReadArgument arg={.kind=SILPH_READ_ENUM32,.value.bits=(uint32_t)items[i]};SilphReadValue value;
        s->stage=30+i;status=silph_read_call(s->read,s->item_bag,"GetItemCount",SILPH_READ_I32,&arg,1,&value);
        int32_t count=0;if(!status)memcpy(&count,&value.bits,4);
        if(!status&&(count<0||count>100000))status=SILPH_MANAGED_CONTRACT;
        if(!status)output->items[i]=(uint32_t)count;
    }
    SilphReadValue pokemon={0},stops={0};
    if(!status){s->stage=38;status=silph_read_field(s->read,s->directory,"emds",SILPH_READ_REFERENCE,&pokemon);}
    if(!status){s->stage=39;status=silph_read_field(s->read,s->directory,"emdp",SILPH_READ_REFERENCE,&stops);}
    if(!status)status=scalar(s,pokemon.reference,"get_Count",&output->map_pokemon);
    if(!status)status=scalar(s,stops.reference,"get_Count",&output->map_stops);
    if(!status&&!s->compact)status=candidates(s,pokemon.reference,output);
    int a=release(s,&stops.reference),b=release(s,&pokemon.reference);if(!status)status=a?a:b;
    if(!status){output->samples=++s->samples;output->account_hi=s->account_hi;output->account_lo=s->account_lo;s->previous=*output;s->snapshot_ready=1;}
    else memset(output,0,sizeof(*output));return status;
}
int silph_game_read_sample(void *memory,void *map,SilphGameSnapshot *output){return silph_game_read_poll(memory,map,true,output);}
int silph_game_read_refresh(void *memory,bool inventory,SilphGameSnapshot *output){
    GameRead *s=memory;if(!s||!s->opened||!output)return SILPH_MANAGED_ARGUMENT;
    void *map=NULL;int status=silph_managed_target(s->managed,s->map,&map);return status?status:silph_game_read_poll(s,map,inventory,output);
}
static int location(GameRead *s,SilphManagedRef object,double *lat,double *lon) {
    SilphReadValue box={0},a={0},b={0};
    int status=silph_read_call(s->read,object,"get_Location",SILPH_READ_BOXED,NULL,0,&box);
    if(!status)status=silph_read_is_type(s->read,box.reference,"Niantic","LatLng");
    if(!status)status=silph_read_field(s->read,box.reference,"Latitude",SILPH_READ_F64,&a);
    if(!status)status=silph_read_field(s->read,box.reference,"Longitude",SILPH_READ_F64,&b);
    if(!status){memcpy(lat,&a.bits,8);memcpy(lon,&b.bits,8);if(!isfinite(*lat)||!isfinite(*lon)||fabs(*lat)>90||fabs(*lon)>180)status=SILPH_MANAGED_CONTRACT;}
    int freed=release(s,&box.reference);return status?status:freed;
}
static int player_location(GameRead *s,double *lat,double *lon) {
    SilphManagedRef type={0};SilphReadValue avatar={0},ready={0};
    int status=silph_read_type_object(s->read,"holo-game.dll","Niantic.Holoholo.Avatar","IMapAvatar",&type);
    SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=type};
    if(!status)status=silph_read_call(s->read,s->container,"TryResolve",SILPH_READ_REFERENCE,&arg,1,&avatar);
    if(!status)status=silph_read_call(s->read,avatar.reference,"get_InitialLocationSet",SILPH_READ_BOOL,NULL,0,&ready);
    if(!status&&!ready.bits)status=SILPH_MANAGED_NOT_READY;
    if(!status)status=location(s,avatar.reference,lat,lon);
    int a=release(s,&avatar.reference),b=release(s,&type);return status?status:a?a:b;
}
static double distance_meters(double lat,double lon,double other_lat,double other_lon) {
    const double radians=0.017453292519943295;
    double a=sin((lat-other_lat)*radians/2),b=sin((lon-other_lon)*radians/2);
    double value=a*a+cos(lat*radians)*cos(other_lat*radians)*b*b;
    value=fmax(0,fmin(1,value));return 12742000*asin(sqrt(value));
}
int silph_game_read_position(void *memory,double *lat,double *lon){
    GameRead *s=memory;if(!s||!s->opened||!lat||!lon)return SILPH_MANAGED_ARGUMENT;return player_location(s,lat,lon);
}
static int read_text(GameRead *s,SilphManagedRef owner,const char *name,char *output,uint32_t capacity){
    SilphReadValue value={0};int status=silph_read_call(s->read,owner,name,SILPH_READ_REFERENCE,NULL,0,&value);uint32_t length=0;
    if(!status)status=silph_read_utf8(s->read,value.reference,output,capacity,&length);
    if(!status&&!length)status=SILPH_MANAGED_CONTRACT;
    int freed=release(s,&value.reference);return status?status:freed;
}
int silph_game_read_item_bag(void *memory,SilphManagedRef *output){
    GameRead *s=memory;if(!s||!s->opened||!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    void *object=NULL;int status=silph_managed_target(s->managed,s->item_bag,&object);return status?status:silph_managed_retain(s->managed,object,false,output);
}
int silph_game_read_item(void *memory,int32_t item,uint32_t *output){
    GameRead *s=memory;if(!s||!s->opened||!output||item<=0)return SILPH_MANAGED_ARGUMENT;*output=0;
    SilphReadArgument arg={.kind=SILPH_READ_ENUM32,.value.bits=(uint32_t)item};SilphReadValue value={0};
    int status=silph_read_call(s->read,s->item_bag,"GetItemCount",SILPH_READ_I32,&arg,1,&value);
    if(!status&&value.bits>100000)status=SILPH_MANAGED_CONTRACT;if(!status)*output=(uint32_t)value.bits;return status;
}
static int read_stop(void *memory,double maximum,uint32_t incident_type,SilphStopPredicate accept,void *context,const SilphGameStop *target,SilphGameStop *output){
    GameRead *s=memory;if(!s||!s->opened||!output||!isfinite(maximum)||maximum<=0||maximum>80||(incident_type!=0&&incident_type!=8))return SILPH_MANAGED_ARGUMENT;
    memset(output,0,sizeof(*output));output->meters=INFINITY;
    double lat=0,lon=0;int status=player_location(s,&lat,&lon);
    SilphReadValue dictionary={0},values={0},iterator={0},current={0},value={0};
    if(!status)status=silph_read_field(s->read,s->directory,"emdp",SILPH_READ_REFERENCE,&dictionary);
    if(!status)status=silph_read_call(s->read,dictionary.reference,"get_Values",SILPH_READ_REFERENCE,NULL,0,&values);
    if(!status)status=silph_read_call(s->read,values.reference,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&iterator);
    for(unsigned i=0;!status&&i<512;i++){
        status=silph_read_call(s->read,iterator.reference,"MoveNext",SILPH_READ_BOOL,NULL,0,&value);
        if(status||!value.bits){if(!status)output->scan_complete=1;break;}output->scanned++;
        status=silph_read_call(s->read,iterator.reference,"get_Current",SILPH_READ_REFERENCE,NULL,0,&current);
        double a=0,b=0;if(!status)status=location(s,current.reference,&a,&b);double meters=distance_meters(lat,lon,a,b);
        if(!status&&target&&distance_meters(a,b,target->latitude,target->longitude)>target->meters){status=release(s,&current.reference);continue;}
        if(!status&&target&&isfinite(meters)&&meters<=maximum)output->matches++;
        if(!status&&isfinite(meters)&&meters<=maximum&&meters<output->meters){
            SilphGameStop row={.latitude=a,.longitude=b,.player_latitude=lat,.player_longitude=lon,.meters=meters};
            status=read_text(s,current.reference,"get_Id",row.fort,sizeof(row.fort));
            if(!status&&accept&&!accept(context,row.fort)){status=release(s,&current.reference);continue;}
            if(!status&&(incident_type||target)){
                status=silph_read_call(s->read,current.reference,"get_ActiveIncidentType",SILPH_READ_ENUM32,NULL,0,&value);row.incident_type=(uint32_t)value.bits;
                if(!status&&incident_type&&row.incident_type!=incident_type){status=release(s,&current.reference);continue;}
                if(!status)status=silph_read_call(s->read,current.reference,"get_IsIncidentActive",SILPH_READ_BOOL,NULL,0,&value);row.active=(uint32_t)value.bits;
                if(!status)status=silph_read_call(s->read,current.reference,"get_IsIncidentComplete",SILPH_READ_BOOL,NULL,0,&value);row.complete=(uint32_t)value.bits;
                if(!status&&incident_type&&(!row.active||row.complete)){status=release(s,&current.reference);continue;}
                if(!status&&row.incident_type==8&&row.active&&!row.complete){
                if(!status)status=read_text(s,current.reference,"get_CurrentIncidentId",row.incident,sizeof(row.incident));
                SilphManagedRef display={0};
                if(!status)status=silph_read_field_type(s->read,current.reference,"Holoholo.Rpc","PokestopIncidentDisplayProto",&display);
                if(!status)status=silph_read_field(s->read,display,"IncidentDisplayType",SILPH_READ_ENUM32,&value);
                if(!status&&value.bits!=8)status=SILPH_MANAGED_CONTRACT;
                if(!status)status=silph_read_field(s->read,display,"IncidentCompleted",SILPH_READ_BOOL,&value);
                if(!status&&value.bits)status=SILPH_MANAGED_CONTRACT;
                SilphReadValue incident={0};char incident_id[161]={0};uint32_t length=0;
                if(!status)status=silph_read_field(s->read,display,"IncidentId",SILPH_READ_REFERENCE,&incident);
                if(!status)status=silph_read_utf8(s->read,incident.reference,incident_id,sizeof(incident_id),&length);
                if(!status&&strcmp(incident_id,row.incident))status=SILPH_MANAGED_CONTRACT;
                int released=release(s,&incident.reference);if(!status)status=released;
                if(!status)status=silph_read_field(s->read,display,"IncidentExpirationMs",SILPH_READ_I64,&value);
                if(!status)row.expires_ms=value.bits;
                int freed=release(s,&display);if(!status)status=freed;
                }
            }
            if(!status){row.scanned=output->scanned;row.matches=output->matches;*output=row;}
        }
        int freed=release(s,&current.reference);if(!status)status=freed;
    }
    int disposed=0;if(iterator.reference.serial)disposed=silph_read_call(s->read,iterator.reference,"Dispose",SILPH_READ_VOID,NULL,0,&value);
    SilphManagedRef *refs[]={&current.reference,&iterator.reference,&values.reference,&dictionary.reference};
    for(unsigned i=0;i<4;i++){int freed=release(s,refs[i]);if(!status)status=freed;}if(!status)status=disposed;
    if(status){memset(output,0,sizeof(*output));return status;}return output->fort[0]?0:SILPH_MANAGED_NOT_READY;
}
int silph_game_read_stop(void *memory,double maximum,uint32_t type,SilphStopPredicate accept,void *context,SilphGameStop *out){return read_stop(memory,maximum,type,accept,context,NULL,out);}
int silph_game_read_stop_at(void *memory,double maximum,double latitude,double longitude,double tolerance,SilphGameStop *out){
    if(!isfinite(latitude)||!isfinite(longitude)||fabs(latitude)>90||fabs(longitude)>180||!isfinite(tolerance)||tolerance<=0||tolerance>10)return SILPH_MANAGED_ARGUMENT;
    SilphGameStop target={.latitude=latitude,.longitude=longitude,.meters=tolerance};return read_stop(memory,maximum,0,NULL,NULL,&target,out);
}
int silph_game_read_distance(void *memory,SilphManagedRef object,double *meters) {
    GameRead *s=memory;if(!s||!s->opened||!meters)return SILPH_MANAGED_ARGUMENT;*meters=INFINITY;
    double a=0,b=0,c=0,d=0;int status=player_location(s,&a,&b);
    if(!status)status=location(s,object,&c,&d);if(!status)*meters=distance_meters(a,b,c,d);return status;
}
int silph_game_read_select(void *memory,double maximum,SilphPokemonPredicate accept,void *context,SilphGameTarget *output) {
    GameRead *s=memory;if(!s||!s->opened||!output||!isfinite(maximum)||maximum<=0||maximum>50)return SILPH_MANAGED_ARGUMENT;
    memset(output,0,sizeof(*output));output->meters=INFINITY;
    double lat=0,lon=0;int status=player_location(s,&lat,&lon);
    SilphReadValue dictionary={0},collection={0},iterator={0},current={0},value={0};
    if(!status)status=silph_read_field(s->read,s->directory,"emds",SILPH_READ_REFERENCE,&dictionary);
    if(!status)status=silph_read_call(s->read,dictionary.reference,"get_Values",SILPH_READ_REFERENCE,NULL,0,&collection);
    if(!status)status=silph_read_call(s->read,collection.reference,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&iterator);
    for(unsigned i=0;!status&&i<128;i++) {
        status=silph_read_call(s->read,iterator.reference,"MoveNext",SILPH_READ_BOOL,NULL,0,&value);if(status||!value.bits)break;
        status=silph_read_call(s->read,iterator.reference,"get_Current",SILPH_READ_REFERENCE,NULL,0,&current);
        if(!status&&silph_read_is_type(s->read,current.reference,"Niantic.Holoholo.Map","WildMapPokemon")) {status=release(s,&current.reference);continue;}
        if(!status)status=silph_read_call(s->read,current.reference,"get_Destroyed",SILPH_READ_BOOL,NULL,0,&value);
        if(!status&&value.bits){status=release(s,&current.reference);continue;}
        double a=0,b=0;if(!status)status=location(s,current.reference,&a,&b);double meters=distance_meters(lat,lon,a,b);
        if(!status&&isfinite(meters)&&meters<=maximum&&meters<output->meters) {
            status=silph_read_call(s->read,current.reference,"get_EncounterId",SILPH_READ_U64,NULL,0,&value);uint64_t id=value.bits;uint32_t species=0;
            if(!status)status=scalar(s,current.reference,"get_PokemonId",&species);
            if(!status&&(!id||!species))status=SILPH_MANAGED_CONTRACT;
            if(!status&&accept&&!accept(context,id,species)){status=release(s,&current.reference);continue;}
            if(!status)status=release(s,&output->object);
            if(!status){output->object=current.reference;memset(&current.reference,0,sizeof(current.reference));output->encounter=id;output->species=species;output->meters=meters;}
        }
        int freed=release(s,&current.reference);if(!status)status=freed;
    }
    int disposed=0;if(iterator.reference.serial)disposed=silph_read_call(s->read,iterator.reference,"Dispose",SILPH_READ_VOID,NULL,0,&value);
    int a=release(s,&current.reference),b=release(s,&iterator.reference),c=release(s,&collection.reference),d=release(s,&dictionary.reference);
    if(!status)status=disposed?disposed:a?a:b?b:c?c:d;
    if(status){release(s,&output->object);memset(output,0,sizeof(*output));return status;}
    return output->object.serial?0:SILPH_MANAGED_NOT_READY;
}
int silph_game_read_nearest(void *memory,double maximum,SilphGameTarget *output){return silph_game_read_select(memory,maximum,NULL,NULL,output);}
int silph_game_read_owned(void *memory,uint64_t id,SilphManagedRef *output){
    GameRead *s=memory;if(!s||!s->opened||!id||!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    SilphReadArgument arg={.kind=SILPH_READ_U64,.value.bits=id};SilphReadValue value;
    int status=silph_read_call(s->read,s->pokemon_bag,"GetPokemon",SILPH_READ_REFERENCE,&arg,1,&value);
    if(!status)*output=value.reference;return status;
}
static int service(GameRead *s,const char *assembly,const char *space,const char *name,SilphManagedRef *output){
    SilphManagedRef type={0};SilphReadValue value={0};int status=silph_read_type_object(s->read,assembly,space,name,&type);
    SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=type};
    if(!status)status=silph_read_call(s->read,s->container,"TryResolve",SILPH_READ_REFERENCE,&arg,1,&value);
    if(!status&&!value.reference.serial)status=SILPH_MANAGED_CONTRACT;
    if(!status){*output=value.reference;memset(&value.reference,0,sizeof(value.reference));}
    int a=release(s,&type),b=release(s,&value.reference);return status?status:a?a:b;
}
int silph_game_read_rpc_handler(void *memory,SilphManagedRef *output){
    GameRead *s=memory;if(!s||!s->opened||!output)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    return service(s,"holo-game.dll","Niantic.Holoholo.Rpc","IRpcHandler",output);
}
int silph_game_read_service(void *memory,uint32_t kind,SilphManagedRef *output){
    GameRead *s=memory;if(!s||!s->opened||!output||kind>1)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    return kind?service(s,"holo-game.dll","Niantic.Holoholo.Buddy","IBuddyPokemonService",output):service(s,"holo-protos.dll","","IGameMasterData",output);
}
int silph_game_read_gift_service(void *memory,uint32_t kind,SilphManagedRef *output){
    GameRead *s=memory;if(!s||!s->opened||!output||kind>2)return SILPH_MANAGED_ARGUMENT;memset(output,0,sizeof(*output));
    return service(s,kind?"holo-game-interfaces.dll":"holo-game.dll",kind?"Niantic.Holoholo.Social.Gifting":"Niantic.Holoholo.Social",kind==0?"IFriendsRpcService":kind==1?"IGiftingRpcService":"IGiftingService",output);
}
int silph_game_read_map_count(void *memory,uint32_t *output){
    GameRead *s=memory;if(!s||!s->opened||!output)return SILPH_MANAGED_ARGUMENT;SilphReadValue dictionary={0};
    int status=silph_read_field(s->read,s->directory,"emds",SILPH_READ_REFERENCE,&dictionary);if(!status)status=scalar(s,dictionary.reference,"get_Count",output);int freed=release(s,&dictionary.reference);return status?status:freed;
}
#ifdef SILPH_ENABLE_ACTIONS
#include "research-bridge.h"
int silph_game_read_research_service(void *memory,uint32_t kind,SilphManagedRef *out){
    GameRead *s=memory;if(!s||!s->opened||!out||kind>4)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));
    if(kind<2)return service(s,"holo-game.dll","Niantic.Holoholo.Quests",kind?"IQuestRpcService":"IQuestService",out);
    if(kind==3)return service(s,"holo-game-interfaces.dll","Niantic.Holoholo.Quests","IQuestStampCardService",out);
    if(kind==4)return service(s,"holo-game.dll","Niantic.Holoholo.UI.TodayView.EventPass","IEventPassService",out);
    SilphManagedRef type={0};SilphReadValue value={0};int status=silph_research_factory_type(s->read,&type);
    SilphReadArgument arg={.kind=SILPH_READ_REFERENCE,.value.reference=type};
    if(!status)status=silph_read_call(s->read,s->container,"TryResolve",SILPH_READ_REFERENCE,&arg,1,&value);
    if(!status&&!value.reference.serial)status=SILPH_MANAGED_CONTRACT;
    if(!status){*out=value.reference;value.reference=(SilphManagedRef){0};}
    int a=release(s,&type),b=release(s,&value.reference);return status?status:a?a:b;
}
#endif
static int flag_field(GameRead *s,SilphManagedRef pokemon,const char *name,uint32_t flag,SilphTransferInput *data){
    SilphReadValue value;int status=silph_read_field(s->read,pokemon,name,SILPH_READ_BOOL,&value);
    if(!status){data->known_flags|=flag;if(value.bits)data->flags|=flag;}return status;
}
static int flag_call(GameRead *s,SilphManagedRef pokemon,const char *name,uint32_t flag,SilphTransferInput *data){
    SilphReadValue value;int status=silph_read_call(s->read,pokemon,name,SILPH_READ_BOOL,NULL,0,&value);
    if(!status){data->known_flags|=flag;if(value.bits)data->flags|=flag;}return status;
}
static int text_present(GameRead *s,SilphManagedRef pokemon,const char *name,bool *present){
    SilphReadValue text={0},size={0};*present=false;int status=silph_read_field(s->read,pokemon,name,SILPH_READ_REFERENCE,&text);
    if(!status&&text.reference.serial)status=silph_read_call(s->read,text.reference,"get_Length",SILPH_READ_I32,NULL,0,&size);
    if(!status&&size.bits>4096)status=SILPH_MANAGED_CONTRACT;if(!status)*present=size.bits!=0;
    int freed=release(s,&text.reference);return status?status:freed;
}
int silph_game_read_transfer_data(void *memory,uint64_t id,SilphTransferInput *data,SilphManagedRef *bag,SilphManagedRef *pokemon){
    GameRead *s=memory;if(!s||!s->opened||!data||!bag||!pokemon||bag==pokemon)return SILPH_MANAGED_ARGUMENT;
    memset(data,0,sizeof(*data));memset(bag,0,sizeof(*bag));memset(pokemon,0,sizeof(*pokemon));
    int status=silph_game_read_owned(s,id,pokemon);if(!status&&!pokemon->serial)return SILPH_MANAGED_NOT_READY;
    if(!status)status=silph_read_is_type(s->read,*pokemon,"Holoholo.Rpc","PokemonProto");
    SilphReadValue value={0},display={0};SilphManagedRef buddy={0},master={0};
    if(!status)status=silph_read_field(s->read,*pokemon,"id_",SILPH_READ_U64,&value);
    if(!status&&value.bits!=id)status=SILPH_MANAGED_STALE;
    if(!status){data->owned_id=id;data->abi=1;data->valid=SILPH_VALID_ID;data->source=SILPH_SOURCE_WILD;
        data->options_present=data->options_known=data->options=255;}
    if(!status)status=flag_field(s,*pokemon,"isEgg_",SILPH_DATA_EGG,data);
    const char *integers[]={"pokemonId_","individualAttack_","individualDefense_","individualStamina_"};
    int32_t *destinations[]={&data->species,&data->attack,&data->defense,&data->stamina};
    for(unsigned i=0;!status&&i<4;i++){status=silph_read_field(s->read,*pokemon,integers[i],SILPH_READ_I32,&value);if(!status)memcpy(destinations[i],&value.bits,4);}
    if(!status)data->valid|=SILPH_VALID_SPECIES|SILPH_VALID_IV;
    if(!status)status=flag_field(s,*pokemon,"favorite_",SILPH_DATA_FAVORITE,data);
    if(!status)status=flag_field(s,*pokemon,"isFusion_",SILPH_DATA_FUSION,data);
    if(!status)status=flag_field(s,*pokemon,"hatchedFromEgg_",SILPH_DATA_HATCHED,data);
    if(!status)status=flag_call(s,*pokemon,"get_IsShinyPokemon",SILPH_DATA_SHINY,data);
    bool fort=false,station=false,limited=false;
    if(!status)status=text_present(s,*pokemon,"deployedFortId_",&fort);
    if(!status)status=text_present(s,*pokemon,"deployedStationId_",&station);
    if(!status)status=text_present(s,*pokemon,"limitedPokemonIdentifier_",&limited);
    if(!status){data->known_flags|=SILPH_DATA_DEPLOYED|SILPH_DATA_LIMITED;if(fort||station)data->flags|=SILPH_DATA_DEPLOYED;if(limited)data->flags|=SILPH_DATA_LIMITED;}
    if(!status)status=silph_read_field(s->read,*pokemon,"pokemonDisplay_",SILPH_READ_REFERENCE,&display);
    if(!status)status=silph_read_is_type(s->read,display.reference,"Holoholo.Rpc","PokemonDisplayProto");
    if(!status)status=flag_call(s,display.reference,"get_IsAnyBreadPokemon",SILPH_DATA_DYNAMAX,data);
    SilphReadValue card={0},art={0};
    if(!status)status=silph_read_call(s->read,display.reference,"get_HasLocationCard",SILPH_READ_BOOL,NULL,0,&card);
    if(!status)status=silph_read_call(s->read,display.reference,"get_IsNaturalArtPokemon",SILPH_READ_BOOL,NULL,0,&art);
    if(!status){data->known_flags|=SILPH_DATA_BACKGROUND;if(card.bits||art.bits)data->flags|=SILPH_DATA_BACKGROUND;}
    if(!status)status=service(s,"holo-game.dll","Niantic.Holoholo.Buddy","IBuddyPokemonService",&buddy);
    SilphReadArgument arg={.kind=SILPH_READ_U64,.value.bits=id};
    if(!status)status=silph_read_call(s->read,buddy,"GetIsBuddy",SILPH_READ_BOOL,&arg,1,&value);
    if(!status){data->known_flags|=SILPH_DATA_BUDDY;if(value.bits)data->flags|=SILPH_DATA_BUDDY;}
    if(!status)status=service(s,"holo-protos.dll","","IGameMasterData",&master);
    arg=(SilphReadArgument){.kind=SILPH_READ_REFERENCE,.value.reference=master};
    if(!status)status=silph_read_call(s->read,*pokemon,"GetClass",SILPH_READ_ENUM32,&arg,1,&value);
    if(!status){memcpy(&data->pokemon_class,&value.bits,4);data->valid|=SILPH_VALID_CLASS;}
    void *object=NULL;if(!status)status=silph_managed_target(s->managed,s->pokemon_bag,&object);
    if(!status)status=silph_managed_retain(s->managed,object,false,bag);
    int a=release(s,&display.reference),b=release(s,&buddy),c=release(s,&master);if(!status)status=a?a:b?b:c;
    if(status){release(s,bag);release(s,pokemon);memset(data,0,sizeof(*data));}return status;
}
