#define _GNU_SOURCE
#include "farmer-view.h"
#include <shadowhook.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct {
    const void **(*assemblies)(void *,size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *);
    const char *(*class_name)(void *),*(*class_namespace)(void *);
    const void *(*method)(void *,const char *,int),*(*field)(void *,const char *),*(*field_type)(const void *);
    uint32_t (*field_flags)(const void *);
    const char *(*type_name)(const void *);
    void (*free)(void *),(*get)(void *,const void *,void *);
} ViewApi;
typedef struct {
    SilphManagedApi base;ViewApi api;
    void *managed,*context,*allowed_context,*scheduler,*pokemon,*items,*profile,*info,*stores[3],*pokedex,*main_menu;
#ifdef SILPH_PRESENT_SCOPED
    void *notices[2];
#endif
    SilphManagedRef map,gui;
    const void *top,*active,*visible,*selection,*selection_active,*remote;
    void *stub;uintptr_t address;unsigned char before[16];
    SilphVisualAllowed allowed;SilphFarmerPump pump;
    unsigned owner,closed,busy,continuation,rocket,kecleon;uint32_t interval_ms;uint64_t next;
    SilphFarmerViewMetrics metrics;
} View;
static _Atomic(View *) current;
static atomic_uint owner_tid;
static void (*update_original)(void *,const void *);
static uint64_t clock_ns(void){struct timespec t={0};return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static void *klass(View *s,const char *assembly,const char *space,const char *name){
    size_t n=0;const void **all=s->api.assemblies(s->base.domain_get(),&n);if(!all||!n||n>2048)return NULL;
    for(size_t i=0;i<n;i++){const void *image=s->api.image(all[i]);const char *actual=image?s->api.image_name(image):NULL;if(actual&&!strcmp(actual,assembly))return s->api.klass(image,space,name);}return NULL;
}
static const void *method(View *s,void *k,const char *name,int kind){
    const void *m=k?s->api.method(k,name,0):NULL;uint32_t impl=0;
    return m&&s->base.method_get_param_count(m)==0&&!(s->base.method_get_flags(m,&impl)&0x10u)&&!s->base.method_is_generic(m)&&!s->base.method_is_inflated(m)&&s->base.type_get_type(s->base.method_get_return_type(m))==kind?m:NULL;
}
static const void *field(View *s,void *k,const char *name,const char *expected){
    const void *f=k?s->api.field(k,name):NULL;if(!f||(s->api.field_flags(f)&0x50u))return NULL;
    const char *type=s->api.type_name(s->api.field_type(f));bool match=type&&!strcmp(type,expected);if(type)s->api.free((void *)type);return match?f:NULL;
}
static int invoke(View *s,const void *m,void *object,void **out){void *exception=NULL,*result=s->base.runtime_invoke(m,object,NULL,&exception);if(exception)return 1;if(out)*out=result;return 0;}
static int boolean(View *s,const void *m,void *object,bool *out){void *boxed=NULL;*out=false;if(!m||invoke(s,m,object,&boxed)||!boxed)return 1;void *v=s->base.object_unbox(boxed);if(!v)return 1;memcpy(out,v,1);return 0;}
static uint32_t classify(View *s,void *map){
    if(s->rocket)return SILPH_VIEW_MAP;
    bool active=false,visible=false;if(boolean(s,s->active,map,&active)||!active)return 0;
    if(boolean(s,s->visible,map,&visible))return 0;
    void *gui=NULL,*top=NULL;if(silph_managed_target(s->managed,s->gui,&gui)||invoke(s,s->top,gui,&top))return 0;
    if(!top){s->metrics.name[0]=0;return visible?SILPH_VIEW_MAP:0;}
    void *k=s->base.object_get_class(top);const char *name=k?s->api.class_name(k):NULL,*space=k?s->api.class_namespace(k):NULL;
    snprintf(s->metrics.name,sizeof(s->metrics.name),"%s.%s",space?space:"",name?name:"");
    if(s->kecleon)return visible&&k==s->main_menu?SILPH_VIEW_MAP:0;
#ifdef SILPH_PRESENT_SCOPED
    for(unsigned i=0;visible&&k&&i<2;i++)if(k==s->notices[i])return SILPH_VIEW_MAP;
#endif
    if(k==s->pokemon){
        bool remote=true,selected=true;void *buttons=NULL;
        if(boolean(s,s->remote,top,&remote)||remote||!s->selection||!s->selection_active)return 0;
        s->api.get(top,s->selection,&buttons);if(!buttons||boolean(s,s->selection_active,buttons,&selected)||selected)return 0;
        return SILPH_VIEW_POKEMON;
    }
    if(k==s->items)return SILPH_VIEW_ITEMS;
    if(k==s->profile)return SILPH_VIEW_PROFILE;
    if(k&&k==s->info)return SILPH_VIEW_POKEMON_INFO;
    if(k&&k==s->pokedex)return SILPH_VIEW_POKEDEX;
    for(unsigned i=0;i<3;i++)if(k&&k==s->stores[i])return SILPH_VIEW_STORE;
    return visible&&k==s->main_menu?SILPH_VIEW_MAP:0;
}
static void update(void *object,const void *m){
    update_original(object,m);if(atomic_load(&owner_tid)!=(unsigned)syscall(__NR_gettid))return;View *s=atomic_load(&current);
    if(!s||s->closed||s->busy||s->owner!=(unsigned)syscall(__NR_gettid)||s->base.object_get_class(object)!=s->scheduler)return;
    s->metrics.updates++;uint64_t now=clock_ns();if(!now||now<s->next)return;
    uint32_t interval=s->continuation&&s->interval_ms==100?33:s->interval_ms;s->next=now+(uint64_t)interval*UINT64_C(1000000);
    void *map=NULL;if(silph_managed_target(s->managed,s->map,&map)||!map){s->metrics.stage=4;
#if defined(SILPH_PRESENT_SCOPED) || defined(SILPH_PILOT_FRAME)
        s->pump(NULL,false,0);
#endif
        return;}
    s->busy=1;uint32_t kind=s->allowed(s->allowed_context)?classify(s,map):0;s->metrics.kind=kind;
    if(kind){s->metrics.pumps++;if(kind!=SILPH_VIEW_MAP)s->metrics.menu_pumps++;}else s->metrics.skipped++;
    uint64_t cost=clock_ns()-now;if(cost>s->metrics.max_ns)s->metrics.max_ns=cost;s->busy=0;
    s->pump(map,kind!=0,kind);
}
size_t silph_farmer_view_size(void){return sizeof(View);}
bool silph_farmer_view_ready(void *memory){View *s=memory;return s&&!s->closed&&s->metrics.available;}
void silph_farmer_view_rocket(void *memory,bool active){View *s=memory;if(s)s->rocket=active;}
void silph_farmer_view_kecleon(void *memory,bool active){View *s=memory;if(s)s->kecleon=active;}
void silph_farmer_view_interval(void *memory,uint32_t ms){View *s=memory;if(s&&ms>=100&&ms<=3000)s->interval_ms=ms;}
void silph_farmer_view_continuation(void *memory,bool pending){View *s=memory;if(s){if(pending&&!s->continuation&&s->interval_ms==100)s->next=0;s->continuation=pending;}}
int silph_farmer_view_metrics(void *memory,SilphFarmerViewMetrics *out){
    View *s=memory;if(!s||!out)return 1;SilphManagedMetrics m={0};if(s->managed&&silph_managed_metrics(s->managed,&m))return 1;
    s->metrics.hooks=s->stub!=NULL;if(s->managed){s->metrics.created=m.refs_created;s->metrics.released=m.refs_released;s->metrics.remaining=m.live_refs;}*out=s->metrics;return 0;
}
int silph_farmer_view_open(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,void *map,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address,SilphFarmerPump pump){
    if(!memory||bytes!=sizeof(View)||!base||!resolver||!map||!allowed||!address||!pump||offset>120||offset%sizeof(void *)||current)return 1;
    View *s=memory;memset(s,0,bytes);s->base=*base;s->context=context;s->allowed=allowed;s->allowed_context=allowed_context;s->pump=pump;s->owner=(unsigned)syscall(__NR_gettid);s->interval_ms=100;s->metrics.stage=1;
#define LOAD(member,name) do{void *p=resolver("il2cpp_" name,context);if(!p)return 1;memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(class_name,"class_get_name");LOAD(class_namespace,"class_get_namespace");LOAD(method,"class_get_method_from_name");LOAD(field,"class_get_field_from_name");LOAD(field_type,"field_get_type");LOAD(field_flags,"field_get_flags");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(get,"field_get_value");
#undef LOAD
    s->scheduler=klass(s,"Zeppelin.dll","Niantic.Zeppelin.Scheduler","Scheduler");
    s->pokemon=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Pokemon","PokemonInventoryGuiController");
    s->items=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Items","ItemInventoryGuiController");
    s->profile=klass(s,"holo-game.dll","Niantic.Holoholo.Social.UI","PlayerProfileGuiController");
    s->info=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Pokemon","PokemonInfoGuiController");
    s->pokedex=klass(s,"holo-game.dll","Niantic.Holoholo.UI.PokedexV2","PokedexGuiController");
    s->main_menu=klass(s,"holo-game.dll","Niantic.Holoholo.Map.UI","MainMenuGuiController");
#ifdef SILPH_PRESENT_SCOPED
    const char *notices[]={"QuestUpdateToastGuiController","RewardItemToastListGuiController"};
    for(unsigned i=0;i<2;i++)s->notices[i]=klass(s,"holo-game.dll","Niantic.Holoholo.UI",notices[i]);
#endif
    const char *stores[]={"StorefrontGuiController","StorefrontGuiControllerV2","StorefrontGuiControllerLinkout"};
    for(unsigned i=0;i<3;i++)s->stores[i]=klass(s,"holo-game.dll","Niantic.Holoholo.Storefront",stores[i]);
    void *gui=klass(s,"holo-game.dll","Niantic.Holoholo.NUI","GuiLayerService"),*go=klass(s,"UnityEngine.CoreModule.dll","UnityEngine","GameObject");
    s->top=method(s,gui,"TopGui",18);s->active=method(s,s->base.object_get_class(map),"get_IsActiveState",2);s->visible=method(s,s->base.object_get_class(map),"get_IsVisible",2);
    s->remote=method(s,s->pokemon,"get_IsRemoteTradeView",2);s->selection=field(s,s->pokemon,"multiChoiceButtons","UnityEngine.GameObject");s->selection_active=method(s,go,"get_activeInHierarchy",2);
    const void *entry=method(s,s->scheduler,"Update",1),*gui_field=field(s,s->base.object_get_class(map),"ellz","Niantic.Holoholo.NUI.IGuiLayerService");
    if(!s->scheduler||!s->pokemon||!s->items||!s->profile||!s->main_menu||!s->top||!s->active||!s->visible||!entry||!gui_field)return 1;
    s->metrics.stage=3;size_t size=silph_managed_size(4);s->managed=calloc(1,size);if(!s->managed)return 1;
    if(silph_managed_open(s->managed,size,base,4,64)){free(s->managed);s->managed=NULL;return 1;}
    s->metrics.stage=5;
    void *service=NULL;s->api.get(map,gui_field,&service);if(!service||s->base.object_get_class(service)!=gui)return 1;
    if(silph_managed_retain(s->managed,map,true,&s->map)||silph_managed_retain(s->managed,service,true,&s->gui))return 1;
    memcpy(&s->address,(const char *)entry+offset,sizeof(s->address));if((s->address&3)||!address(s->address,context))return 1;
    memcpy(s->before,(void *)s->address,16);atomic_store(&owner_tid,s->owner);atomic_store(&current,s);s->stub=shadowhook_hook_func_addr((void *)s->address,(void *)update,(void **)&update_original);
    if(!s->stub){atomic_store(&current,NULL);atomic_store(&owner_tid,0);return 1;}s->metrics.available=1;s->metrics.stage=2;return 0;
}
int silph_farmer_view_close(void *memory,SilphFarmerViewMetrics *out){
    View *s=memory;if(!s||s->owner!=(unsigned)syscall(__NR_gettid)||s->busy)return 1;s->closed=1;
    if(s->stub){if(shadowhook_unhook(s->stub)||memcmp((void *)s->address,s->before,16))return 1;s->stub=NULL;}
    SilphManagedRef *refs[]={&s->gui,&s->map};for(unsigned i=0;i<2;i++)if(refs[i]->serial){if(silph_managed_release(s->managed,*refs[i]))return 1;memset(refs[i],0,sizeof(*refs[i]));}
    SilphManagedMetrics m={0};if(s->managed){if(silph_managed_close(s->managed,&m))return 1;free(s->managed);s->managed=NULL;}
    if(atomic_load(&current)==s){atomic_store(&current,NULL);atomic_store(&owner_tid,0);}s->metrics.available=0;s->metrics.hooks=0;s->metrics.remaining=m.live_refs;s->metrics.created=m.refs_created;s->metrics.released=m.refs_released;if(out)*out=s->metrics;return 0;
}
