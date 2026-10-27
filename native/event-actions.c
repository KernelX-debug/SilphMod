#include "event-actions.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include <android/log.h>
typedef struct {void *read,*managed,*game;SilphManagedRef service,selected_session;uint64_t next,scans,taps,scan_ns,max_scan_ns,examined;uint32_t opened,disabled,stage;struct {uint64_t cell,encounter,until;uint32_t id;} seen[128];uint32_t head;} Events;
static uint64_t scan_clock(void){struct timespec t={0};return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
size_t silph_event_size(void){return sizeof(Events);}
static int drop(Events *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)*ref=(SilphManagedRef){0};return r;}
static int call(Events *s,SilphManagedRef o,const char *name,uint32_t kind,const SilphReadArgument *args,uint32_t n,SilphReadValue *out){return silph_read_call(s->read,o,name,kind,args,n,out);}
static int ref(Events *s,SilphManagedRef o,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=call(s,o,name,SILPH_READ_REFERENCE,NULL,0,&v);if(!r)*out=v.reference;return r;}
static int scalar(Events *s,SilphManagedRef o,const char *name,uint32_t kind,uint64_t *out){SilphReadValue v={0};int r=call(s,o,name,kind,NULL,0,&v);if(!r)*out=v.bits;return r;}
static int field(Events *s,SilphManagedRef o,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,o,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;return r;}
static int location(Events *s,SilphManagedRef o,double *lat,double *lon){SilphReadValue box={0},a={0},b={0};int r=call(s,o,"get_Location",SILPH_READ_BOXED,NULL,0,&box);
    if(!r)r=silph_read_is_type(s->read,box.reference,"Niantic","LatLng");if(!r)r=silph_read_field(s->read,box.reference,"Latitude",SILPH_READ_F64,&a);if(!r)r=silph_read_field(s->read,box.reference,"Longitude",SILPH_READ_F64,&b);
    if(!r){memcpy(lat,&a.bits,8);memcpy(lon,&b.bits,8);if(!isfinite(*lat)||fabs(*lat)>90||!isfinite(*lon)||fabs(*lon)>180)r=SILPH_MANAGED_CONTRACT;}int d=drop(s,&box.reference);return r?r:d;}
int silph_event_open(void *memory,size_t bytes,void *read,void *managed,void *game){if(!memory||bytes!=sizeof(Events)||!read||!managed||!game)return SILPH_MANAGED_ARGUMENT;Events *s=memory;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->opened=1;return 0;}
static int candidate(Events *s,SilphManagedRef o,uint64_t wall,double lat,double lon,SilphManagedRef *chosen,double *nearest){
    s->stage=20;
    if(silph_read_is_type(s->read,o,"","DynamicTappableController"))return 0;
    uint64_t visible=0,expires=0,encounter=0,cell=0,id=0;int r=scalar(s,o,"get_EncounterId",SILPH_READ_U64,&encounter);if(!r)r=scalar(s,o,"get_CellId",SILPH_READ_U64,&cell);if(!r)r=scalar(s,o,"get_Id",SILPH_READ_I32,&id);if(r)return r;
    if(!cell||!encounter)return 0;
    for(unsigned i=0;i<128;i++)if(s->seen[i].cell==cell&&s->seen[i].encounter==encounter&&s->seen[i].id==(uint32_t)id&&s->seen[i].until>wall)return 0;
    r=scalar(s,o,"get_VisibleOnMap",SILPH_READ_BOOL,&visible);if(!r&&visible)r=scalar(s,o,"get_DespawnTimeMs",SILPH_READ_I64,&expires);if(r)return r;
    if(!visible||(expires&&expires<=wall))return 0;
    s->stage=21;double a=0,b=0;r=location(s,o,&a,&b);if(r)return r;double dy=(a-lat)*111195,dx=remainder(b-lon,360.0)*111195*cos(lat*0.017453292519943295),meters=hypot(dx,dy);if(meters>50||meters>=*nearest)return 0;
    s->stage=22;SilphManagedRef key={0};char name[160]={0};uint32_t length=0;r=ref(s,o,"get_TypeKey",&key);if(!r)r=silph_read_utf8(s->read,key,name,sizeof(name),&length);int d=drop(s,&key);if(!r)r=d;if(r)return r;
    if(strcmp(name,"TAPPABLE_TYPE_BOX")||!encounter)return 0;
    r=drop(s,chosen);if(!r){void *object=NULL;r=silph_managed_target(s->managed,o,&object);if(!r)r=silph_managed_retain(s->managed,object,false,chosen);if(!r)*nearest=meters;}return r;
}
static int scan(Events *s,uint64_t wall,SilphManagedRef *chosen){
    double lat=0,lon=0,nearest=INFINITY;int r=silph_game_read_position(s->game,&lat,&lon);if(r)return r;
    SilphManagedRef sessions={0},it={0},pair={0},session={0},list={0},item={0};SilphReadValue v={0};r=field(s,s->service,"deny",&sessions);
    if(!r)r=call(s,sessions,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&v);it=v.reference;
    unsigned examined=0;
    for(unsigned n=0;!r&&n<8&&examined<128;n++){
        uint64_t more=0;r=scalar(s,it,"MoveNext",SILPH_READ_BOOL,&more);if(r||!more)break;
        r=call(s,it,"get_Current",SILPH_READ_BOXED,NULL,0,&v);pair=v.reference;if(!r)r=ref(s,pair,"get_Value",&session);
        if(!r)r=field(s,session,"visibleTappables",&list);uint64_t count=0;if(!r)r=scalar(s,list,"get_Count",SILPH_READ_I32,&count);if(!r&&count>4096)r=SILPH_MANAGED_CONTRACT;
        for(unsigned i=0;!r&&i<count&&examined++<128;i++){
            SilphReadArgument arg={.kind=SILPH_READ_I32,.value.bits=i};r=call(s,list,"get_Item",SILPH_READ_REFERENCE,&arg,1,&v);item=v.reference;double before=nearest;if(!r&&item.serial)r=candidate(s,item,wall,lat,lon,chosen,&nearest);
            if(!r&&nearest<before){r=drop(s,&s->selected_session);void *owner=NULL;if(!r)r=silph_managed_target(s->managed,session,&owner);if(!r)r=silph_managed_retain(s->managed,owner,false,&s->selected_session);}int d=drop(s,&item);if(!r)r=d;
        }
        SilphManagedRef *refs[]={&list,&session,&pair};for(unsigned i=0;i<3;i++){int d=drop(s,refs[i]);if(!r)r=d;}
    }
    s->examined+=examined;SilphManagedRef *refs[]={&item,&list,&session,&pair,&it,&sessions};for(unsigned i=0;i<6;i++){int d=drop(s,refs[i]);if(!r)r=d;}return r;
}
int silph_event_tick(void *memory,const SilphSchedulePermit *permit,uint64_t now,uint64_t wall,bool actions){
    Events *s=memory;if(!s||!s->opened||!silph_schedule_permitted(permit,true))return SILPH_MANAGED_STATE;if(s->disabled||now<s->next)return 0;s->next=now+UINT64_C(2000000000);
    int r=0;s->stage=1;if(!s->service.serial)r=silph_game_read_event_service(s->game,&s->service);SilphManagedRef chosen={0};if(!r){s->stage=2;uint64_t started=scan_clock();r=scan(s,wall,&chosen);uint64_t ended=scan_clock(),spent=ended>=started?ended-started:0;s->scan_ns+=spent;if(spent>s->max_scan_ns)s->max_scan_ns=spent;}s->scans++;
    if(!r&&actions&&chosen.serial){s->stage=30;uint64_t cell=0,id=0,encounter=0,expires=0;r=scalar(s,chosen,"get_CellId",SILPH_READ_U64,&cell);if(!r)r=scalar(s,chosen,"get_Id",SILPH_READ_I32,&id);if(!r)r=scalar(s,chosen,"get_EncounterId",SILPH_READ_U64,&encounter);if(!r)r=scalar(s,chosen,"get_DespawnTimeMs",SILPH_READ_I64,&expires);
        if(!r){s->stage=31;r=silph_event_expect(encounter);}if(!r){s->stage=32;r=silph_event_tap(s->read,permit,s->selected_session,chosen);}
        if(!r||(s->stage==32&&r==SILPH_MANAGED_EXCEPTION)){unsigned slot=s->head++%128;s->seen[slot].cell=cell;s->seen[slot].id=(uint32_t)id;s->seen[slot].encounter=encounter;s->seen[slot].until=expires?expires:wall+UINT64_C(600000);s->taps++;__android_log_print(ANDROID_LOG_INFO,"SilphEvent","interaction requested encounter=%llu sequence=%llu result=%d",(unsigned long long)encounter,(unsigned long long)s->taps,r);}
        else if(s->stage!=32||r!=SILPH_MANAGED_EXCEPTION)silph_event_complete(encounter);
    }
    int d=drop(s,&chosen),owner=drop(s,&s->selected_session);if(!r)r=d?d:owner;if(r){s->next=now+UINT64_C(30000000000);__android_log_print(ANDROID_LOG_INFO,"SilphEvent","event actions deferred error=%d stage=%u scans=%llu",r,s->stage,(unsigned long long)s->scans);}return 0;
}
int silph_event_close(void *memory){Events *s=memory;if(!s||!s->opened)return 0;int r=drop(s,&s->selected_session),d=drop(s,&s->service);if(!r)r=d;__android_log_print(ANDROID_LOG_INFO,"SilphEvent","event actions closed scans=%llu requested=%llu examined=%llu scanNs=%llu maxScanNs=%llu remaining=%u",(unsigned long long)s->scans,(unsigned long long)s->taps,(unsigned long long)s->examined,(unsigned long long)s->scan_ns,(unsigned long long)s->max_scan_ns,!!s->service.serial+!!s->selected_session.serial);if(!r)memset(s,0,sizeof(*s));return r;}
