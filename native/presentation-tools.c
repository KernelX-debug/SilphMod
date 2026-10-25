#define _GNU_SOURCE
#include "presentation-tools.h"
#include "game-read.h"
#include <shadowhook.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>
enum {SCENE_BADGE_INCREMENT,SCENE_INCENSE,SCENE_LEVEL,SCENE_XP,SCENE_EGG,SCENE_EGG_CUTSCENE,SCENE_SPEED,SCENE_STAMP,SCENE_FRIENDSHIP,SCENE_POKEMON_RESULT,SCENE_BADGE_AWARD,SCENE_GYM_AWARD,SCENE_VIVILLON_AWARD,SCENE_AVATAR_AWARD,SCENE_COUNT};
typedef struct {const char *space,*name,*action;} SceneSpec;
static const SceneSpec scene_specs[SCENE_COUNT]={
    {"Niantic.Holoholo.UI","BadgeIncrementGuiController","Dismiss"},
    {"Items.PlayerBuffs.Incense","DailyIncenseRecapGuiController","Dismiss"},
    {"Niantic.Holoholo.UI","LevelUpV3GuiController","Dismiss"},
    {"Niantic.Holoholo.UI","XpAwardGuiControllerV2","LuckyBonusSkipButtonClicked"},
    {"Niantic.Holoholo.UI","EggHatchGuiController","Dismiss"},
    {"Niantic.Holoholo.Cutscenes","EggHatchCutsceneGuiController","Dismiss"},
    {"Niantic.Holoholo.Map","SpeedModal","Dismiss"},
    {"Niantic.Holoholo.UI","NewStampCollectionGuiController","Dismiss"},
    {"Niantic.Holoholo.Notifications","FriendshipLevelUpGuiController","EndLevelUpFlow"},
    {"Niantic.Holoholo.UI.Pokemon","PokemonInfoGuiController","DismissPokemonInfoPanel"},
    {"Niantic.Holoholo.UI","BadgeAwardGuiController","Dismiss"},
    {"Niantic.Holoholo.GymBadges","GymBadgeAwardGuiController","Dismiss"},
    {"Niantic.Holoholo.Vivillon","VivillonSubBadgeAwardGuiController","DismissButtonHit"},
    {"Niantic.Holoholo.Map.UI","AvatarCustomizationAwardGuiController","Dismiss"}
};
typedef struct {
    const void **(*assemblies)(void *,size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *);
    void *(*parent)(void *),*(*nested)(void *,void **);
    bool (*assignable)(void *,void *);
    bool (*is_enum)(void *);
    void *(*class_from_type)(const void *);
    const void *(*enum_base)(void *);
    const void *(*method)(void *,const char *,int),*(*parameter)(const void *,uint32_t);
    const char *(*type_name)(const void *);
    void (*free)(void *);
    const void *(*field)(void *,const char *),*(*field_type)(const void *);
    uint32_t (*field_flags)(const void *);
    void (*get)(void *,const void *,void *),(*set)(void *,const void *,void *);
    void *(*object_new)(void *);
} Api;
typedef struct {void *stub;uintptr_t address;unsigned char bytes[16];} Hook;
typedef struct {void *klass,*asset_class;const void *action,*finish,*asset,*skip,*asset_alive;uint32_t failed;} Scene;
typedef struct {SilphManagedRef object;unsigned target,attempts,requested;uint64_t until,next;} Queued;
typedef struct {SilphManagedRef object;float before;uint32_t animator;} FloatRecord;
typedef struct {
    unsigned owner,closed,offset,busy,depth,primed,operations;
    uint32_t available,failed,stage;
    SilphManagedApi base;Api api;void *managed,*context,*allowed_context;
    SilphVisualAllowed allowed;SilphVisualAddress address;
    Hook hooks[12];unsigned hooks_used;
    void *gui,*map_class,*layer_class,*result_data_class;const void *state,*alive,*top,*map_layer,*layer_top,*result_data,*result_confirm;Scene scenes[SCENE_COUNT];Queued queue[16];uint64_t observe_after;
    void *gift,*box_stage,*box,*reward,*animator,*wait,*iterators[2];
    const void *duration,*gift_animator,*stage_box,*box_animator,*reward_animator,*wait_seconds,*iterator_current[2],*get_speed,*set_speed,*gift_alive,*animator_alive;
    FloatRecord records[48];unsigned record_count;
    void *pokemon,*display,*multi,*list;const void *flags[4],*id,*limited,*display_field,*shiny,*selection_disabled,*clone,*mode,*selected,*remote,*is_buddy;
    SilphManagedRef master,buddy;
    uint64_t scenes_count,eggs,gifts,waits,selections,batches,open_drains;
    uint32_t last_scene,scene_supported,scene_errors;uint64_t scene_retries,scene_timeouts;
} Presentation;
static _Atomic(Presentation *) current;
static atomic_uint owner_tid;
static Presentation *owned(void){return atomic_load(&owner_tid)==(unsigned)syscall(__NR_gettid)?atomic_load(&current):NULL;}
static void (*open_original)(void *,const void *),(*egg_original)(void *,void *,const void *);
static bool (*top_original)(void *,const void *),(*selection_original)(void *,void *,const void *);
static void (*click_original)(void *,const void *),(*dismiss_original)(void *,const void *),(*stage_original)(void *,const void *),(*reward_original)(void *,void *,const void *);
static bool (*iterator_original[2])(void *,const void *);
static void (*batch_original)(void *,int32_t,void *,void *,void *,const void *);
size_t silph_presentation_size(void){return sizeof(Presentation);}
bool silph_presentation_busy(void *memory){Presentation *s=memory;return s&&(s->operations||s->busy||s->depth);}
static bool operation(Presentation *s){if(!s||s->owner!=(unsigned)syscall(__NR_gettid)||s->closed)return false;s->operations++;return true;}
static bool allowed(Presentation *s,uint32_t feature){return s&&s->owner==(unsigned)syscall(__NR_gettid)&&!s->closed&&(s->available&feature)&&s->allowed(s->allowed_context);}
static void fail(Presentation *s,uint32_t feature,uint32_t stage){s->available&=~feature;s->failed|=feature;s->stage=stage;}
static void *klass(Presentation *s,const char *assembly,const char *space,const char *name){
    size_t n=0;const void **all=s->api.assemblies(s->base.domain_get(),&n);if(!all||!n||n>2048)return NULL;
    for(size_t i=0;i<n;i++){const void *image=s->api.image(all[i]);const char *actual=image?s->api.image_name(image):NULL;if(actual&&!strcmp(actual,assembly))return s->api.klass(image,space,name);}return NULL;
}
static bool type(Presentation *s,const void *t,const char *expected){const char *name=t?s->api.type_name(t):NULL;bool equal=name&&!strcmp(name,expected);if(name)s->api.free((void *)name);return equal;}
static const void *field(Presentation *s,void *k,const char *name,const char *expected){const void *f=k?s->api.field(k,name):NULL;return f&&!(s->api.field_flags(f)&0x50u)&&type(s,s->api.field_type(f),expected)?f:NULL;}
static bool signature(Presentation *s,const void *m,const char *result,unsigned count,const char **parameters){
    uint32_t impl=0;if(!m||s->base.method_get_param_count(m)!=count||(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||!type(s,s->base.method_get_return_type(m),result))return false;
    for(unsigned i=0;i<count;i++)if(!type(s,s->api.parameter(m,i),parameters[i]))return false;return true;
}
static const void *method(Presentation *s,void *k,const char *name,const char *result,unsigned count,const char **parameters){const void *m=k?s->api.method(k,name,(int)count):NULL;return signature(s,m,result,count,parameters)?m:NULL;}
static int invoke(Presentation *s,const void *m,void *object,void **args,void **out){void *exception=NULL,*result=s->base.runtime_invoke(m,object,args,&exception);if(exception)return 1;if(out)*out=result;return 0;}
static int unbox(Presentation *s,void *boxed,void *out,size_t bytes){void *value=boxed?s->base.object_unbox(boxed):NULL;if(!value)return 1;memcpy(out,value,bytes);return 0;}
static bool exact(Presentation *s,void *object,void *k){return object&&k&&s->base.object_get_class(object)==k;}
static bool live(Presentation *s,void *object){if(!object||!s->api.assignable(s->gui,s->base.object_get_class(object)))return false;uintptr_t value=0;s->api.get(object,s->alive,&value);return value!=0;}
static int hook(Presentation *s,const void *m,void *proxy,void **original){
    if(!m||s->hooks_used>=12)return 1;uintptr_t address=0;memcpy(&address,(const char *)m+s->offset,sizeof(address));if((address&3)||!s->address(address,s->context))return 1;
    Hook *h=&s->hooks[s->hooks_used];h->address=address;memcpy(h->bytes,(void *)address,16);h->stub=shadowhook_hook_func_addr((void *)address,proxy,original);if(!h->stub)return 1;s->hooks_used++;return 0;
}
static void drop(Presentation *s,SilphManagedRef *ref){if(ref->serial){silph_managed_release(s->managed,*ref);memset(ref,0,sizeof(*ref));}}
static void enqueue(Presentation *s,void *object){
    if(!allowed(s,SILPH_PRESENT_SCENES)||s->busy||!live(s,object))return;unsigned target=SCENE_COUNT;
    for(unsigned i=0;i<SCENE_COUNT;i++)if(!s->scenes[i].failed&&s->scenes[i].klass&&s->base.object_get_class(object)==s->scenes[i].klass){target=i;break;}if(target==SCENE_COUNT)return;
    if(target==SCENE_POKEMON_RESULT){void *data=NULL;uint8_t confirm=0;if(!s->result_data||!s->result_confirm)return;s->api.get(object,s->result_data,&data);if(!exact(s,data,s->result_data_class))return;s->api.get(data,s->result_confirm,&confirm);if(!confirm)return;}
    Queued *slot=NULL;for(unsigned i=0;i<16;i++){void *old=NULL;if(s->queue[i].object.serial&&!silph_managed_target(s->managed,s->queue[i].object,&old)&&old==object)return;if(!s->queue[i].object.serial&&!slot)slot=&s->queue[i];}
    if(slot&&!silph_managed_retain(s->managed,object,true,&slot->object)){slot->target=target;slot->attempts=0;slot->requested=0;slot->until=0;slot->next=0;}
}
void silph_presentation_map(void *memory,void *map,uint64_t now){
    Presentation *s=memory;if(!allowed(s,SILPH_PRESENT_SCENES)||silph_presentation_busy(s)||!now||now<s->observe_after)return;
    s->observe_after=now+UINT64_C(100000000);
    if(!s->map_layer||!s->layer_top||!exact(s,map,s->map_class))return;
    void *layer=NULL,*top=NULL;s->api.get(map,s->map_layer,&layer);
    if(exact(s,layer,s->layer_class)&&!invoke(s,s->layer_top,layer,NULL,&top)){enqueue(s,top);silph_presentation_drain(s,now);}
}
static void on_open(void *object,const void *m){
    Presentation *s=owned();bool entered=operation(s);open_original(object,m);
    if(entered){
        enqueue(s,object);
        if(allowed(s,SILPH_PRESENT_SCENES)&&!s->busy){struct timespec at={0};if(!clock_gettime(CLOCK_MONOTONIC,&at)){
            s->open_drains++;silph_presentation_drain(s,(uint64_t)at.tv_sec*UINT64_C(1000000000)+(uint64_t)at.tv_nsec);
        }}
        s->operations--;
    }
}
static bool on_top(void *object,const void *m){Presentation *s=owned();bool entered=operation(s);bool result=top_original(object,m);if(result)enqueue(owned(),object);if(entered)s->operations--;return result;}
static void on_egg(void *object,void *data,const void *m){Presentation *s=owned();if(allowed(s,SILPH_PRESENT_SCENES)&&!s->busy){s->eggs++;return;}egg_original(object,data,m);}
static void scene_complete(Presentation *s,Queued *q){if(q->requested){s->scenes_count++;s->last_scene=q->target+1;s->scene_errors&=~(1u<<q->target);}drop(s,&q->object);}
static void scene_retry(Presentation *s,Queued *q,uint64_t now,int error){
    if(error){s->scene_errors|=1u<<q->target;s->stage=110+q->target;}
    if(q->attempts<3){s->scene_retries++;q->next=now+UINT64_C(250000000);}else q->next=q->until;
}
void silph_presentation_drain(void *memory,uint64_t now){
    Presentation *s=memory;if(!allowed(s,SILPH_PRESENT_SCENES)||!now||s->busy)return;s->busy=1;unsigned actions=0;
    for(unsigned i=0;i<16;i++){Queued *q=&s->queue[i];if(!q->object.serial)continue;if(!q->until)q->until=now+UINT64_C(30000000000);
        void *object=NULL;if(silph_managed_target(s->managed,q->object,&object)||!live(s,object)){scene_complete(s,q);continue;}
        int32_t state=0;s->api.get(object,s->state,&state);if(state==0||state==3){scene_complete(s,q);continue;}
        if(now>=q->until){s->scene_timeouts++;s->scene_errors|=1u<<q->target;s->stage=130+q->target;drop(s,&q->object);continue;}
        if(actions>=2||now<q->next||q->attempts>=3||state!=2)continue;
        void *boxed=NULL;bool top=false;if(invoke(s,s->top,object,NULL,&boxed)||unbox(s,boxed,&top,1)){q->attempts++;actions++;scene_retry(s,q,now,1);continue;}if(!top)continue;
        Scene *scene=&s->scenes[q->target];int error=0;if(scene->asset){void *asset=NULL;s->api.get(object,scene->asset,&asset);if(exact(s,asset,scene->asset_class)){uintptr_t native=0;s->api.get(asset,scene->asset_alive,&native);if(native)error=invoke(s,scene->skip,asset,NULL,NULL);}}
        if(!error)error=invoke(s,scene->action,object,NULL,NULL);
        if(!error&&scene->finish&&allowed(s,SILPH_PRESENT_SCENES)){void *fresh=NULL;uint32_t active=0;if(!silph_managed_target(s->managed,q->object,&fresh)&&live(s,fresh)){s->api.get(fresh,s->state,&active);if(active==2)error=invoke(s,scene->finish,fresh,NULL,NULL);}}
        actions++;q->attempts++;if(error){scene_retry(s,q,now,error);continue;}q->requested=1;
        void *fresh=NULL;if(silph_managed_target(s->managed,q->object,&fresh)||!live(s,fresh)){scene_complete(s,q);continue;}
        s->api.get(fresh,s->state,&state);if(state==0||state==3)scene_complete(s,q);else scene_retry(s,q,now,0);
    }s->busy=0;
}
static int floating(Presentation *s,void *object,bool animator,float *value,bool write){
    const void *alive=animator?s->animator_alive:s->gift_alive;if(alive){uintptr_t native=0;s->api.get(object,alive,&native);if(!native)return 2;}
    if(animator){void *boxed=NULL;if(!exact(s,object,s->animator))return 1;if(write){void *args[]={value};return invoke(s,s->set_speed,object,args,NULL);}if(invoke(s,s->get_speed,object,NULL,&boxed)||unbox(s,boxed,value,4))return 1;}
    else{if(!exact(s,object,s->gift))return 1;if(write)s->api.set(object,s->duration,value);else s->api.get(object,s->duration,value);}return !isfinite(*value)||*value<0||*value>1000;
}
static int remember(Presentation *s,void *object,bool animator){
    if(!object)return 0;float before=0,replacement=animator?12.0f:.05f;int read=floating(s,object,animator,&before,false);if(read==2)return 0;if(read)return 1;if(before==replacement)return 0;
    FloatRecord *record=NULL;for(unsigned i=0;i<s->record_count;i++){void *old=NULL;if(!silph_managed_target(s->managed,s->records[i].object,&old)&&old==object){record=&s->records[i];break;}if(!old&&!record){drop(s,&s->records[i].object);record=&s->records[i];}}
    if(!record){if(s->record_count==48)return 1;record=&s->records[s->record_count++];}
    if(!record->object.serial){if(silph_managed_retain(s->managed,object,true,&record->object))return 1;record->before=before;record->animator=animator;}
    return floating(s,object,animator,&replacement,true);
}
static int restore(Presentation *s){
    int error=0;for(unsigned i=0;i<s->record_count;i++){FloatRecord *r=&s->records[i];void *object=NULL;float actual=0,replacement=r->animator?12.0f:.05f;
        if(r->object.serial&&!silph_managed_target(s->managed,r->object,&object)&&object){int read=floating(s,object,r->animator,&actual,false);if(read&&read!=2)error=1;else if(!read&&actual==replacement&&floating(s,object,r->animator,&r->before,true))error=1;}drop(s,&r->object);
    }s->record_count=0;return error;
}
static void gift_click(void *object,const void *m){Presentation *s=owned();bool entered=operation(s);if(allowed(s,SILPH_PRESENT_GIFTS)&&exact(s,object,s->gift)){void *animator=NULL;s->api.get(object,s->gift_animator,&animator);if(remember(s,object,false)||remember(s,animator,true))fail(s,SILPH_PRESENT_GIFTS,201);else s->gifts++;}click_original(object,m);if(entered)s->operations--;}
static void gift_dismiss(void *object,const void *m){Presentation *s=owned();bool entered=operation(s);dismiss_original(object,m);if(entered){if(exact(s,object,s->gift)&&restore(s))fail(s,SILPH_PRESENT_GIFTS,202);s->operations--;}}
static void gift_stage(void *object,const void *m){Presentation *s=owned();bool entered=operation(s);if(allowed(s,SILPH_PRESENT_GIFTS)&&exact(s,object,s->box_stage)){void *box=NULL,*animator=NULL;s->api.get(object,s->stage_box,&box);if(box&&exact(s,box,s->box)){s->api.get(box,s->box_animator,&animator);if(remember(s,animator,true))fail(s,SILPH_PRESENT_GIFTS,203);}}stage_original(object,m);if(entered)s->operations--;}
static void gift_reward(void *object,void *data,const void *m){Presentation *s=owned();bool entered=operation(s);bool accelerate=allowed(s,SILPH_PRESENT_GIFTS)&&s->depth&&exact(s,object,s->reward);reward_original(object,data,m);if(accelerate){void *animator=NULL;s->api.get(object,s->reward_animator,&animator);if(remember(s,animator,true))fail(s,SILPH_PRESENT_GIFTS,204);}if(entered)s->operations--;}
static bool gift_next(void *object,const void *m,unsigned index){
    Presentation *s=owned();bool active=allowed(s,SILPH_PRESENT_GIFTS)&&exact(s,object,s->iterators[index]);if(active)s->depth++;bool result=iterator_original[index](object,m);
    if(active){if(result&&allowed(s,SILPH_PRESENT_GIFTS)){void *wait=NULL;s->api.get(object,s->iterator_current[index],&wait);if(exact(s,wait,s->wait)){float duration=0;s->api.get(wait,s->wait_seconds,&duration);if(isfinite(duration)&&duration>.051f&&duration<=5){float short_wait=.05f;s->api.set(wait,s->wait_seconds,&short_wait);s->waits++;}}}s->depth--;}return result;
}
static bool gift_next0(void *o,const void *m){return gift_next(o,m,0);}static bool gift_next1(void *o,const void *m){return gift_next(o,m,1);}
static int blocked(Presentation *s,void *pokemon,bool *out){
    *out=true;if(!exact(s,pokemon,s->pokemon)||!s->primed)return 1;uint8_t values[4]={0};for(unsigned i=0;i<4;i++)s->api.get(pokemon,s->flags[i],&values[i]);if(values[0]||values[1]||values[2])return 0;
    void *limited=NULL;s->api.get(pokemon,s->limited,&limited);if(limited&&s->base.string_length(limited)>0)return 0;
    void *display=NULL;s->api.get(pokemon,s->display_field,&display);uint8_t shiny=0;if(display){if(!exact(s,display,s->display))return 1;s->api.get(display,s->shiny,&shiny);}
    void *checked=pokemon,*copy_display=NULL;SilphManagedRef copy={0};int error=0;
    if(shiny||values[3]){if(invoke(s,s->clone,pokemon,NULL,&checked)||!checked||checked==pokemon||!exact(s,checked,s->pokemon))return 1;
        error=silph_managed_retain(s->managed,checked,false,&copy);uint8_t no=0;if(!error)s->api.set(checked,s->flags[3],&no);
        if(!error&&shiny){s->api.get(checked,s->display_field,&copy_display);if(!exact(s,copy_display,s->display)||copy_display==display)error=1;else s->api.set(copy_display,s->shiny,&no);}
    }
    void *master=NULL,*buddy=NULL,*boxed=NULL;bool disabled=true;uint64_t id=0;
    if(!error)error=silph_managed_target(s->managed,s->master,&master);if(!error){void *args[]={master};error=invoke(s,s->selection_disabled,checked,args,&boxed);if(!error)error=unbox(s,boxed,&disabled,1);}drop(s,&copy);if(error||disabled)return error;
    s->api.get(pokemon,s->id,&id);if(!id)return 1;error=silph_managed_target(s->managed,s->buddy,&buddy);
    if(!error){void *args[]={&id};error=invoke(s,s->is_buddy,buddy,args,&boxed);if(!error)error=unbox(s,boxed,out,1);}return error;
}
static bool manual_selection(void *object,void *pokemon,const void *m){
    Presentation *s=owned();bool entered=operation(s),result=selection_original(object,pokemon,m);
    if(allowed(s,SILPH_PRESENT_MANUAL)&&exact(s,object,s->multi)&&s->primed){int32_t mode=1;s->api.get(object,s->mode,&mode);if(!mode){bool deny=true;if(blocked(s,pokemon,&deny))fail(s,SILPH_PRESENT_MANUAL,301);else{s->selections++;result=deny;}}}
    if(entered)s->operations--;return result;
}
static int filtered(Presentation *s,void *object,void **output,SilphManagedRef *held){
    void *selected=NULL;s->api.get(object,s->selected,&selected);if(!selected)return 1;void *k=s->base.object_get_class(selected);const void *kt=s->base.class_get_type(k);if(!type(s,kt,"System.Collections.Generic.List<Holoholo.Rpc.PokemonProto>"))return 1;
    const char *one_int[]={"System.Int32"},*one_mon[]={"Holoholo.Rpc.PokemonProto"};
    const void *count=method(s,k,"get_Count","System.Int32",0,NULL),*item=method(s,k,"get_Item","Holoholo.Rpc.PokemonProto",1,one_int),*ctor=method(s,k,".ctor","System.Void",0,NULL),*add=method(s,k,"Add","System.Void",1,one_mon);
    if(!count||!item||!ctor||!add)return 1;void *boxed=NULL;int32_t n=0;if(invoke(s,count,selected,NULL,&boxed)||unbox(s,boxed,&n,4)||n<0||n>10000)return 1;
    uint64_t *ids=n?calloc((size_t)n,sizeof(*ids)):NULL;if(n&&!ids)return 1;void *replacement=s->api.object_new(k);int error=replacement?silph_managed_retain(s->managed,replacement,false,held):1;unsigned used=0;
    if(!error)error=invoke(s,ctor,replacement,NULL,NULL);
    for(int32_t i=0;!error&&i<n;i++){void *pokemon=NULL,*args[]={&i};error=invoke(s,item,selected,args,&pokemon);bool deny=true;if(!error)error=blocked(s,pokemon,&deny);if(error||deny)continue;uint64_t id=0;s->api.get(pokemon,s->id,&id);bool duplicate=false;for(unsigned j=0;j<used;j++)if(ids[j]==id)duplicate=true;if(duplicate)continue;ids[used++]=id;void *values[]={pokemon};error=invoke(s,add,replacement,values,NULL);}
    free(ids);if(!error){*output=replacement;s->batches++;}return error;
}
static void manual_batch(void *object,int32_t button,void *selection,void *buttons,void *other,const void *m){
    Presentation *s=owned();bool entered=operation(s);void *replacement=selection;SilphManagedRef held={0};
    if(allowed(s,SILPH_PRESENT_MANUAL)&&s->primed&&exact(s,object,s->list)){uint8_t remote=1;s->api.get(object,s->remote,&remote);if(!remote&&filtered(s,object,&replacement,&held)){fail(s,SILPH_PRESENT_MANUAL,302);drop(s,&held);if(entered)s->operations--;return;}}
    batch_original(object,button,replacement,buttons,other,m);if(entered){drop(s,&held);s->operations--;}
}
static void setup_scenes(Presentation *s){
    s->gui=klass(s,"holo-game.dll","Niantic.Holoholo.NUI","GuiController");s->alive=field(s,s->gui,"m_CachedPtr","System.IntPtr");s->state=s->gui?s->api.field(s->gui,"currentState"):NULL;
    void *state_class=s->state?s->api.class_from_type(s->api.field_type(s->state)):NULL;
    if(!s->state||(s->api.field_flags(s->state)&0x50u)||!state_class||!s->api.is_enum(state_class)||!type(s,s->api.enum_base(state_class),"System.Int32"))s->state=NULL;
    s->top=method(s,s->gui,"get_IsTopGui","System.Boolean",0,NULL);const void *opened=method(s,s->gui,"<Open>b__84_2","System.Void",0,NULL);
    if(!s->gui||!s->alive||!s->state||!s->top||!opened){fail(s,SILPH_PRESENT_SCENES,100);return;}
    for(unsigned i=0;i<SCENE_COUNT;i++){Scene *scene=&s->scenes[i];const SceneSpec *spec=&scene_specs[i];scene->klass=klass(s,"holo-game.dll",spec->space,spec->name);scene->action=method(s,scene->klass,spec->action,"System.Void",0,NULL);scene->failed=!scene->action||!scene->klass||!s->api.assignable(s->gui,scene->klass);}
    s->scenes[SCENE_FRIENDSHIP].finish=method(s,s->scenes[SCENE_FRIENDSHIP].klass,"Dismiss","System.Void",0,NULL);if(!s->scenes[SCENE_FRIENDSHIP].finish)s->scenes[SCENE_FRIENDSHIP].failed=1;
    s->result_data_class=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Pokemon","PokemonInfoData");s->result_data=field(s,s->scenes[SCENE_POKEMON_RESULT].klass,"egbx","Niantic.Holoholo.UI.Pokemon.PokemonInfoData");s->result_confirm=field(s,s->result_data_class,"ShowConfirmButton","System.Boolean");if(!s->result_data||!s->result_confirm)s->scenes[SCENE_POKEMON_RESULT].failed=1;
    unsigned valid=0;for(unsigned i=0;i<SCENE_COUNT;i++){if(s->scenes[i].failed)s->scene_errors|=1u<<i;else{s->scene_supported|=1u<<i;valid++;}}
    Scene *egg=&s->scenes[SCENE_EGG_CUTSCENE];void *asset=klass(s,"holo-game.dll","","PokemonInfoEggHatch");egg->asset_class=asset;egg->asset_alive=field(s,asset,"m_CachedPtr","System.IntPtr");egg->asset=field(s,egg->klass,"eggHatchAsset","PokemonInfoEggHatch");egg->skip=method(s,asset,"SkipEggHatchAnimation","System.Void",0,NULL);if(!egg->asset||!egg->skip||!egg->asset_alive){egg->asset=NULL;egg->skip=NULL;}
    if(!valid||hook(s,opened,(void *)on_open,(void **)&open_original)||hook(s,s->top,(void *)on_top,(void **)&top_original)){fail(s,SILPH_PRESENT_SCENES,101);return;}s->available|=SILPH_PRESENT_SCENES;
    void *map=klass(s,"holo-game.dll","Niantic.Holoholo.Map","MapExploreState"),*iterator=NULL;const void *unique=NULL;const char *parameter[]={"Niantic.Holoholo.EggHatch.HatchedPokemonsData"};unsigned matches=0;
    s->map_class=map;s->layer_class=klass(s,"holo-game.dll","Niantic.Holoholo.NUI","GuiLayerService");
    s->map_layer=field(s,map,"ellz","Niantic.Holoholo.NUI.IGuiLayerService");s->layer_top=method(s,s->layer_class,"TopGui","Niantic.Holoholo.NUI.IGuiController",0,NULL);
    for(unsigned i=0;map&&i<2048;i++){const void *m=s->base.class_get_methods(map,&iterator);if(!m)break;if(signature(s,m,"System.Void",1,parameter)){unique=m;matches++;}}
    if(matches==1&&hook(s,unique,(void *)on_egg,(void **)&egg_original))s->stage=102;
}
static void setup_gifts(Presentation *s){
    s->gift=klass(s,"holo-game.dll","Niantic.Holoholo.Social.Gifting.UI","OpenGiftGuiController");s->box_stage=klass(s,"holo-game.dll","Niantic.Holoholo.Social.Gifting.UI","GiftBoxStage");s->box=klass(s,"holo-game.dll","","GiftBox");s->reward=klass(s,"holo-game.dll","Niantic.Holoholo.UI","MajorRewardSequenceView");s->animator=klass(s,"UnityEngine.AnimationModule.dll","UnityEngine","Animator");s->wait=klass(s,"Zeppelin.dll","Niantic.Zeppelin.Scheduler","Wait");
    s->duration=field(s,s->gift,"giftOpenAnimationLength","System.Single");s->gift_animator=field(s,s->gift,"guiAnimator","UnityEngine.Animator");s->stage_box=field(s,s->box_stage,"dloi","GiftBox");s->box_animator=field(s,s->box,"giftBoxAnimator","UnityEngine.Animator");s->reward_animator=field(s,s->reward,"animator","UnityEngine.Animator");s->wait_seconds=field(s,s->wait,"<SleepFor>k__BackingField","System.Single");
    s->gift_alive=field(s,s->gift,"m_CachedPtr","System.IntPtr");s->animator_alive=field(s,s->animator,"m_CachedPtr","System.IntPtr");
    const char *single[]={"System.Single"},*reward_data[]={"Niantic.Holoholo.UI.MajorRewardSequenceView.MajorRewardSequenceData"};
    s->get_speed=method(s,s->animator,"get_speed","System.Single",0,NULL);s->set_speed=method(s,s->animator,"set_speed","System.Void",1,single);
    const void *methods[6]={method(s,s->gift,"OnOpenClick","System.Void",0,NULL),method(s,s->gift,"HandleDismissComplete","System.Void",0,NULL),method(s,s->box_stage,"AnimateGiftBoxOpen","System.Void",0,NULL),method(s,s->reward,"Initialize","System.Void",1,reward_data),NULL,NULL};
    void *iterator=NULL;for(unsigned i=0;s->gift&&i<512;i++){void *k=s->api.nested(s->gift,&iterator);if(!k)break;const char *name=s->api.type_name(s->base.class_get_type(k));if(name){if(strstr(name,"OpenGiftGuiController.ddh"))s->iterators[0]=k;if(strstr(name,"OpenGiftGuiController.ddi"))s->iterators[1]=k;s->api.free((void *)name);}}
    const char *fields[]={"dlrn","dlrq"};bool valid=s->duration&&s->gift_animator&&s->stage_box&&s->box_animator&&s->reward_animator&&s->wait_seconds&&s->get_speed&&s->set_speed&&s->gift_alive&&s->animator_alive;
    for(unsigned i=0;i<2;i++){s->iterator_current[i]=field(s,s->iterators[i],fields[i],"Niantic.Zeppelin.Scheduler.ISchedule");methods[4+i]=method(s,s->iterators[i],"MoveNext","System.Boolean",0,NULL);valid=valid&&s->iterator_current[i];}
    for(unsigned i=0;i<6;i++)valid=valid&&methods[i];if(!valid){fail(s,SILPH_PRESENT_GIFTS,200);return;}
    void *proxies[]={(void *)gift_click,(void *)gift_dismiss,(void *)gift_stage,(void *)gift_reward,(void *)gift_next0,(void *)gift_next1};void **originals[]={(void **)&click_original,(void **)&dismiss_original,(void **)&stage_original,(void **)&reward_original,(void **)&iterator_original[0],(void **)&iterator_original[1]};
    for(unsigned i=0;i<6;i++)if(hook(s,methods[i],proxies[i],originals[i])){fail(s,SILPH_PRESENT_GIFTS,205);return;}s->available|=SILPH_PRESENT_GIFTS;
}
static void setup_manual(Presentation *s){
    s->pokemon=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");s->display=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonDisplayProto");s->multi=klass(s,"holo-game.dll","Niantic.Holoholo.UI","PokemonInventoryMultiSelect");s->list=klass(s,"holo-game.dll","Niantic.Holoholo.UI","PokemonInventoryListLine");
    const char *flags[]={"favorite_","isEgg_","isFusion_","isLucky_"};bool valid=true;for(unsigned i=0;i<4;i++){s->flags[i]=field(s,s->pokemon,flags[i],"System.Boolean");valid=valid&&s->flags[i];}
    s->id=field(s,s->pokemon,"id_","System.UInt64");s->limited=field(s,s->pokemon,"limitedPokemonIdentifier_","System.String");s->display_field=field(s,s->pokemon,"pokemonDisplay_","Holoholo.Rpc.PokemonDisplayProto");s->shiny=field(s,s->display,"Shiny","System.Boolean");
    const char *master[]={"IGameMasterData"},*mon[]={"Holoholo.Rpc.PokemonProto"},*batch[]={"System.Int32","System.Collections.Generic.List<Holoholo.Rpc.PokemonProto>","Niantic.Holoholo.UI.IModalButton[]","Niantic.Holoholo.UI.IModalButton[]"};
    s->selection_disabled=method(s,s->pokemon,"IsSelectionDisabled","System.Boolean",1,master);s->clone=method(s,s->pokemon,"Clone","Holoholo.Rpc.PokemonProto",0,NULL);s->mode=field(s,s->multi,"ebpm","Niantic.Holoholo.UI.SelectMode");s->selected=field(s,s->list,"<SelectedPokemon>k__BackingField","System.Collections.Generic.List<Holoholo.Rpc.PokemonProto>");s->remote=field(s,s->list,"<IsRemoteTrade>k__BackingField","System.Boolean");
    const void *gate=method(s,s->multi,"cdjt","System.Boolean",1,mon),*send=method(s,s->list,"cdio","System.Void",4,batch);
    valid=valid&&s->id&&s->limited&&s->display_field&&s->shiny&&s->selection_disabled&&s->clone&&s->mode&&s->selected&&s->remote&&gate&&send;
    if(!valid||hook(s,gate,(void *)manual_selection,(void **)&selection_original)||hook(s,send,(void *)manual_batch,(void **)&batch_original)){fail(s,SILPH_PRESENT_MANUAL,300);return;}s->available|=SILPH_PRESENT_MANUAL;
}
int silph_presentation_open(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,uint32_t flags,SilphVisualAllowed callback,void *allowed_context,SilphVisualAddress address){
    if(!memory||bytes!=sizeof(Presentation)||!base||!resolver||!callback||!address||current||offset>120||offset%sizeof(void *))return 1;Presentation *s=memory;memset(s,0,bytes);s->base=*base;s->context=context;s->allowed=callback;s->allowed_context=allowed_context;s->address=address;s->offset=offset;s->owner=(unsigned)syscall(__NR_gettid);
#define LOAD(member,name) do{void *p=resolver("il2cpp_" name,context);if(!p)return 1;memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(parent,"class_get_parent");LOAD(nested,"class_get_nested_types");LOAD(assignable,"class_is_assignable_from");LOAD(is_enum,"class_is_enum");LOAD(class_from_type,"class_from_type");LOAD(enum_base,"class_enum_basetype");LOAD(method,"class_get_method_from_name");LOAD(parameter,"method_get_param");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(field,"class_get_field_from_name");LOAD(field_type,"field_get_type");LOAD(field_flags,"field_get_flags");LOAD(get,"field_get_value");LOAD(set,"field_set_value");LOAD(object_new,"object_new");
#undef LOAD
    size_t size=silph_managed_size(256);s->managed=calloc(1,size);if(!s->managed)return 1;int error=silph_managed_open(s->managed,size,base,256,64);if(error){free(s->managed);s->managed=NULL;return error;}atomic_store(&owner_tid,s->owner);current=s;
    if(flags&SILPH_PRESENT_SCENES)setup_scenes(s);if(flags&SILPH_PRESENT_GIFTS)setup_gifts(s);if(flags&SILPH_PRESENT_MANUAL)setup_manual(s);return 0;
}
int silph_presentation_prime(void *memory,void *game,void *managed){
    Presentation *s=memory;if(!s||s->primed||!(s->available&SILPH_PRESENT_MANUAL)||!allowed(s,SILPH_PRESENT_MANUAL))return 0;SilphManagedRef master={0},buddy={0};void *a=NULL,*b=NULL;int error=silph_game_read_service(game,0,&master);if(!error)error=silph_game_read_service(game,1,&buddy);
    if(!error)error=silph_managed_target(managed,master,&a);if(!error)error=silph_managed_target(managed,buddy,&b);if(!error)error=silph_managed_retain(s->managed,a,false,&s->master);if(!error)error=silph_managed_retain(s->managed,b,false,&s->buddy);
    const char *parameter[]={"System.UInt64"};if(!error){s->is_buddy=method(s,s->base.object_get_class(b),"GetIsBuddy","System.Boolean",1,parameter);if(!s->is_buddy)error=1;}
    if(master.serial)silph_managed_release(managed,master);if(buddy.serial)silph_managed_release(managed,buddy);if(error)fail(s,SILPH_PRESENT_MANUAL,303);else s->primed=1;return error;
}
int silph_presentation_metrics(void *memory,SilphPresentationMetrics *out){
    Presentation *s=memory;if(!s||!out)return 1;SilphManagedMetrics m={0};if(s->managed&&silph_managed_metrics(s->managed,&m))return 1;unsigned hooks=0;for(unsigned i=0;i<s->hooks_used;i++)if(s->hooks[i].stub)hooks++;
    unsigned pending=0;for(unsigned i=0;i<16;i++)if(s->queue[i].object.serial)pending++;
    *out=(SilphPresentationMetrics){.available=s->available,.failed=s->failed,.stage=s->stage,.remaining_hooks=hooks,.scenes=s->scenes_count,.eggs=s->eggs,.gifts=s->gifts,.waits=s->waits,.selections=s->selections,.batches=s->batches,.created=m.refs_created,.released=m.refs_released,.remaining=m.live_refs,.open_drains=s->open_drains,.last_scene=s->last_scene,.pending=pending,.scene_supported=s->scene_supported,.scene_errors=s->scene_errors,.scene_retries=s->scene_retries,.scene_timeouts=s->scene_timeouts};return 0;
}
int silph_presentation_close(void *memory,SilphPresentationMetrics *out){
    Presentation *s=memory;if(!s||s->owner!=(unsigned)syscall(__NR_gettid)||silph_presentation_busy(s))return 1;s->closed=1;int error=0;
    for(unsigned i=0;i<s->hooks_used;i++){Hook *h=&s->hooks[i];if(h->stub){if(shadowhook_unhook(h->stub)||memcmp((void *)h->address,h->bytes,16))error=1;else h->stub=NULL;}}if(error)return error;
    error=restore(s);for(unsigned i=0;i<16;i++)drop(s,&s->queue[i].object);drop(s,&s->master);drop(s,&s->buddy);SilphManagedMetrics m={0};if(s->managed){int closed=silph_managed_close(s->managed,&m);if(closed)error=closed;}
    if(out){silph_presentation_metrics(s,out);out->created=m.refs_created;out->released=m.refs_released;out->remaining=m.live_refs;}if(!error){free(s->managed);s->managed=NULL;current=NULL;}return error;
}
