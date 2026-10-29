#define _GNU_SOURCE
#include "visual-tools.h"
#include "callback-memory.h"
#include <shadowhook.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include <math.h>
#include <android/log.h>
#include <time.h>
#define VISUAL_REFS 512u
#define TRAINER 128u
#define IV_NAMES 256u
typedef struct {
    const void **(*assemblies)(void *,size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *);
    const void *(*method)(void *,const char *,int);
    const void *(*parameter)(const void *,uint32_t);
    void *(*class_from_type)(const void *);
    bool (*assignable)(void *,void *),(*value_type)(void *);
    const char *(*type_name)(const void *);
    void (*free)(void *);
    const void *(*field)(void *,const char *);
    uint32_t (*field_flags)(const void *);
    const void *(*field_type)(const void *);
    void (*field_get)(void *,const void *,void *);
} Api;
typedef struct {void *stub;uintptr_t address;unsigned char bytes[16];bool observe;} Hook;
typedef struct {SilphManagedRef text,original;} TextRecord;
typedef struct {float r,g,b,a;} TextColor;
typedef struct {SilphManagedRef text;TextColor before,applied;} ColorRecord;
typedef struct {SilphManagedRef object;uint64_t after;uint32_t panel;bool red;} PendingColor;
typedef struct {
    atomic_uint owner;unsigned closed,managed_ready,depth;uint32_t requested,available,failed;
    SilphManagedApi base;Api api;void *managed,*context,*allowed_context;
    SilphVisualAllowed allowed,startup_allowed;void *startup_context;SilphVisualAddress address;
    void *pokemon,*text_class,*hud,*profile,*friends;
    const void *iv[3],*egg,*text_alive,*hud_fields[2],*profile_field,*friends_field;
    const void *get_text,*set_text,*get_hud;unsigned hud_primed,first_init,first_visible;
    Hook hooks[8];unsigned hooks_used;
    void *cell,*panel,*initializer;const void *cell_name,*panel_name,*initializer_pokemon,*get_color,*set_color;
    ColorRecord colors[128];unsigned color_count,color_supported;
    PendingColor pending_colors[64];unsigned color_cursor;
    SilphManagedRef label,labels[128];unsigned keys[128],next_label;
    TextRecord texts[48];unsigned text_count;
    uint64_t iv_reads,name_updates;
} Visual;
static _Atomic(Visual *) current;
static void *(*nickname_original)(void *,const void *);
static void (*hud_init_original)(void *,const void *),(*hud_visible_original)(void *,bool,const void *);
static void (*hud_info_original)(void *,void *,const void *),(*profile_original)(void *,const void *),(*friends_original)(void *,const void *);
int silph_visual_iv_label(int a,int d,int s,bool egg,char *out,size_t capacity){
    if(!out||capacity<22||egg||a<0||a>15||d<0||d>15||s<0||s>15)return 1;
    int n=snprintf(out,capacity,"%d%% %d/%d/%d",(a+d+s)*100/45,a,d,s);return n<0||(size_t)n>=capacity;
}
size_t silph_visual_size(void){return sizeof(Visual);}
static bool enabled(Visual *s,uint32_t bit){return s&&!s->closed&&(s->available&bit)&&atomic_load(&s->owner)==(unsigned)syscall(__NR_gettid)
    &&(s->allowed(s->allowed_context)||(bit==TRAINER&&s->startup_allowed&&s->startup_allowed(s->startup_context)));}
static int prepare_managed(Visual *s){
    if(s->managed_ready)return 0;int r=silph_managed_open(s->managed,silph_managed_size(VISUAL_REFS),&s->base,VISUAL_REFS,64);
    if(!r){s->managed_ready=1;if(s->requested&TRAINER)r=silph_managed_ascii(s->managed,"SilphMod",8,&s->label);}return r;
}
static Visual *callback_visual(void){
    Visual *s=atomic_load_explicit(&current,memory_order_acquire);if(!s||s->closed)return NULL;unsigned tid=(unsigned)syscall(__NR_gettid),owner=atomic_load(&s->owner);
    if(!owner){char name[16]={0};if(prctl(PR_GET_NAME,name)||strcmp(name,"UnityMain")||!s->base.thread_current())return NULL;
        unsigned expected=0;if(!atomic_compare_exchange_strong(&s->owner,&expected,tid))return NULL;owner=tid;}
    if(owner!=tid)return NULL;if(prepare_managed(s)){s->failed|=s->requested;s->available&=~s->requested;return NULL;}return s;
}
typedef struct {Visual *state;} VisualScope;
static void visual_scope_leave(VisualScope *v){if(v->state&&v->state->depth)v->state->depth--;}
static VisualScope visual_scope(Visual *s){if(s)s->depth++;return(VisualScope){s};}
#define VISUAL_SCOPE(s) VisualScope visual_guard __attribute__((unused,cleanup(visual_scope_leave)))=visual_scope(s)
bool silph_visual_busy(void *memory){Visual *s=memory;return s&&s->depth;}
static void *klass(Visual *s,const char *assembly,const char *space,const char *name){
    size_t count=0;const void **all=s->api.assemblies(s->base.domain_get(),&count);if(!all||!count||count>2048)return NULL;
    for(size_t i=0;i<count;i++){const void *image=s->api.image(all[i]);const char *actual=image?s->api.image_name(image):NULL;if(actual&&!strcmp(actual,assembly))return s->api.klass(image,space,name);}return NULL;
}
static bool type(Visual *s,const void *t,const char *expected){
    const char *name=t?s->api.type_name(t):NULL;bool equal=name&&!strcmp(name,expected);if(name)s->api.free((void *)name);return equal;
}
static const void *field(Visual *s,void *k,const char *name,const char *expected){
    const void *f=k?s->api.field(k,name):NULL;return f&&!(s->api.field_flags(f)&(0x10u|0x40u))&&type(s,s->api.field_type(f),expected)?f:NULL;
}
static const void *method(Visual *s,void *k,const char *name,const char *result,const char *parameter){
    const void *m=k?s->api.method(k,name,parameter?1:0):NULL;uint32_t impl=0;
    if(!m||s->base.method_get_param_count(m)!=(parameter?1u:0u)||(s->base.method_get_flags(m,&impl)&0x10u)
        ||s->base.method_is_generic(m)||s->base.method_is_inflated(m)||!type(s,s->base.method_get_return_type(m),result)
        ||(parameter&&!type(s,s->api.parameter(m,0),parameter)))return NULL;
    return m;
}
static int hook(Visual *s,const void *method,uint32_t offset,void *proxy,void **original){
    if(!method||s->hooks_used>=8)return 1;uintptr_t address=0;memcpy(&address,(const char *)method+offset,sizeof(address));
    if((address&3)||!s->address(address,s->context))return 1;
    Hook *h=&s->hooks[s->hooks_used];h->address=address;memcpy(h->bytes,(const void *)address,16);
    h->stub=shadowhook_hook_func_addr((void *)address,proxy,original);if(!h->stub)return 1;s->hooks_used++;return 0;
}
static int observe(Visual *s,const void *method,uint32_t offset,shadowhook_interceptor_t callback){
    if(!method||s->hooks_used>=8)return 1;uintptr_t address=0;memcpy(&address,(const char *)method+offset,sizeof(address));
    if((address&3)||!s->address(address,s->context))return 1;Hook *h=&s->hooks[s->hooks_used];h->address=address;memcpy(h->bytes,(const void *)address,16);
    h->stub=shadowhook_intercept_func_addr((void *)address,callback,s,SHADOWHOOK_INTERCEPT_WITH_FPSIMD_READ_WRITE);if(!h->stub)return 1;h->observe=true;s->hooks_used++;return 0;
}
static int invoke(Visual *s,const void *method,void *object,void **args,void **out){
    void *exception=NULL,*result=s->base.runtime_invoke(method,object,args,&exception);if(exception)return 1;if(out)*out=result;return 0;
}
static bool alive(Visual *s,void *text){
    if(!text||!s->api.assignable(s->text_class,s->base.object_get_class(text)))return false;uintptr_t ptr=0;s->api.field_get(text,s->text_alive,&ptr);return ptr!=0;
}
static void release(Visual *s,SilphManagedRef *ref){if(ref->serial){silph_managed_release(s->managed,*ref);memset(ref,0,sizeof(*ref));}}
static int mask_text(Visual *s,void *text){
    if(!alive(s,text))return 0;void *label=NULL;if(silph_managed_target(s->managed,s->label,&label))return 1;
    void *before=NULL;if(invoke(s,s->get_text,text,NULL,&before))return 1;if(before==label)return 0;
    TextRecord *record=NULL;
    for(unsigned i=0;i<s->text_count;i++){void *old=NULL;
        int status=silph_managed_target(s->managed,s->texts[i].text,&old);
        if(!status&&old==text){record=&s->texts[i];break;}
        if(status||!alive(s,old)){release(s,&s->texts[i].text);release(s,&s->texts[i].original);if(!record)record=&s->texts[i];}
    }
    if(!record){if(s->text_count==48)return 1;record=&s->texts[s->text_count++];}
    if(!record->text.serial&&silph_managed_retain(s->managed,text,true,&record->text))return 1;
    release(s,&record->original);
    if(before&&silph_managed_retain(s->managed,before,false,&record->original))return 1;
    void *args[]={label};if(invoke(s,s->set_text,text,args,NULL))return 1;s->name_updates++;return 0;
}
static void mask(Visual *s,void *object,void *expected,const void *a,const void *b){
    if(!enabled(s,TRAINER)||!silph_callback_object(object,expected))return;
    void *text=NULL;s->api.field_get(object,a,&text);int status=mask_text(s,text);
    if(!status&&b){text=NULL;s->api.field_get(object,b,&text);status=mask_text(s,text);}
    if(status){s->available&=~TRAINER;s->failed|=TRAINER;}
}
static void hud_init(void *o,const void *m){Visual *s=callback_visual();VISUAL_SCOPE(s);hud_init_original(o,m);if(s){mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);if(!s->first_init++){__android_log_print(ANDROID_LOG_INFO,"SilphVisual","first HUD initialized alias=%u",enabled(s,TRAINER)&&s->name_updates>0);}}}
static void hud_visible(void *o,bool v,const void *m){Visual *s=callback_visual();VISUAL_SCOPE(s);hud_visible_original(o,v,m);if(s&&v){mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);if(!s->first_visible++)__android_log_print(ANDROID_LOG_INFO,"SilphVisual","first HUD visible alias=%u",enabled(s,TRAINER)&&s->name_updates>0);}}
static void hud_info(void *o,void *v,const void *m){Visual *s=callback_visual();VISUAL_SCOPE(s);hud_info_original(o,v,m);if(s)mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);}
static void profile(void *o,const void *m){Visual *s=callback_visual();VISUAL_SCOPE(s);profile_original(o,m);if(s)mask(s,o,s->profile,s->profile_field,NULL);}
static void friends(void *o,const void *m){Visual *s=callback_visual();VISUAL_SCOPE(s);friends_original(o,m);if(s)mask(s,o,s->friends,s->friends_field,NULL);}
static void color_tick(Visual *);
void silph_visual_prime(void *memory,void *map){
    Visual *s=memory;color_tick(s);if(!enabled(s,TRAINER)||s->hud_primed||!map||!s->get_hud)return;
    void *hud=NULL;if(invoke(s,s->get_hud,map,NULL,&hud)||!hud)return;
    mask(s,hud,s->hud,s->hud_fields[0],s->hud_fields[1]);s->hud_primed=1;
}
static void *nickname(void *o,const void *m){
    Visual *s=callback_visual();VISUAL_SCOPE(s);void *original=nickname_original(o,m);if(!enabled(s,IV_NAMES)||!silph_callback_object(o,s->pokemon))return original;
    uint8_t egg=0;int values[3]={0};s->api.field_get(o,s->egg,&egg);if(egg)return original;
    for(unsigned i=0;i<3;i++)s->api.field_get(o,s->iv[i],&values[i]);char text[32];
    if(silph_visual_iv_label(values[0],values[1],values[2],false,text,sizeof(text)))return original;
    unsigned key=(unsigned)(values[0]*256+values[1]*16+values[2])+1,slot=128;
    for(unsigned i=0;i<128;i++)if(s->keys[i]==key){slot=i;break;}
    if(slot==128){slot=s->next_label++%128;release(s,&s->labels[slot]);s->keys[slot]=0;
        if(silph_managed_ascii(s->managed,text,(uint32_t)strlen(text),&s->labels[slot])){s->failed|=IV_NAMES;s->available&=~IV_NAMES;return original;}s->keys[slot]=key;
    }
    void *label=NULL;if(silph_managed_target(s->managed,s->labels[slot],&label)){s->failed|=IV_NAMES;s->available&=~IV_NAMES;return original;}s->iv_reads++;return label;
}
bool silph_visual_hundo(int a,int d,int stamina,bool egg){return !egg&&a==15&&d==15&&stamina==15;}
static bool hundo(Visual *s,void *pokemon){
    if(!silph_callback_object(pokemon,s->pokemon))return false;int v[3]={0};uint8_t egg=0;s->api.field_get(pokemon,s->egg,&egg);
    for(unsigned i=0;i<3;i++)s->api.field_get(pokemon,s->iv[i],&v[i]);return silph_visual_hundo(v[0],v[1],v[2],egg!=0);
}
static bool same_color(TextColor a,TextColor b){return fabsf(a.r-b.r)<.0001f&&fabsf(a.g-b.g)<.0001f&&fabsf(a.b-b.b)<.0001f&&fabsf(a.a-b.a)<.0001f;}
static int color_read(Visual *s,void *text,TextColor *color){void *boxed=NULL;if(invoke(s,s->get_color,text,NULL,&boxed))return 1;void *value=boxed?s->base.object_unbox(boxed):NULL;if(!value)return 1;memcpy(color,value,sizeof(*color));return !isfinite(color->r)||!isfinite(color->g)||!isfinite(color->b)||!isfinite(color->a);}
static int color_write(Visual *s,void *text,TextColor color){void *args[]={&color};return invoke(s,s->set_color,text,args,NULL);}
static void tint(Visual *s,void *text,bool red){
    if(!alive(s,text)||!s->get_color||!s->set_color)return;ColorRecord *record=NULL;
    for(unsigned i=0;i<s->color_count;i++){void *old=NULL;if(silph_managed_target(s->managed,s->colors[i].text,&old)||!alive(s,old)){release(s,&s->colors[i].text);s->colors[i]=(ColorRecord){0};}
        if(old==text){record=&s->colors[i];break;}}
    if(!red&&!record)return;TextColor actual={0};if(color_read(s,text,&actual))return;
    if(!red){if(same_color(actual,record->applied))color_write(s,text,record->before);release(s,&record->text);*record=(ColorRecord){0};return;}
    if(!record){for(unsigned i=0;i<s->color_count;i++)if(!s->colors[i].text.serial){record=&s->colors[i];break;}
        if(!record){if(s->color_count==128)return;record=&s->colors[s->color_count++];}
        if(silph_managed_retain(s->managed,text,true,&record->text))return;record->before=actual;
    }else if(!same_color(actual,record->applied))record->before=actual;
    TextColor red_color={1,0,0,actual.a};if(!color_write(s,text,red_color))record->applied=red_color;
}
static uint64_t color_now(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*UINT64_C(1000000000)+t.tv_nsec;}
static void color_enqueue(Visual *s,void *object,bool red,bool panel){
    PendingColor *slot=NULL;for(unsigned i=0;i<64;i++){PendingColor *entry=&s->pending_colors[i];if(!entry->object.serial){if(!slot)slot=entry;continue;}void *existing=NULL;if(!silph_managed_target(s->managed,entry->object,&existing)&&existing==object){slot=entry;break;}}
    if(!slot)return;if(!slot->object.serial&&silph_managed_retain(s->managed,object,true,&slot->object))return;slot->red=red;slot->panel=panel;slot->after=color_now()+UINT64_C(1000000);
}
static void color_tick(Visual *s){
    if(!enabled(s,IV_NAMES)||!s->color_supported)return;uint64_t now=color_now();unsigned used=0;
    for(unsigned i=0;i<64&&used<8;i++){PendingColor *entry=&s->pending_colors[s->color_cursor++%64];if(!entry->object.serial||now<entry->after)continue;used++;
        void *object=NULL,*text=NULL;if(!silph_managed_target(s->managed,entry->object,&object)&&silph_callback_object(object,entry->panel?s->panel:s->cell)){s->api.field_get(object,entry->panel?s->panel_name:s->cell_name,&text);tint(s,text,entry->red);}
        release(s,&entry->object);*entry=(PendingColor){0};
    }
}
static void cell_init(shadowhook_cpu_context_t *registers,void *data){
    Visual *s=callback_visual();VISUAL_SCOPE(s);void *object=(void *)(uintptr_t)registers->regs[0],*input=(void *)(uintptr_t)registers->regs[1];
    if(s!=data||!enabled(s,IV_NAMES)||!s->color_supported||!silph_callback_object(object,s->cell))return;
    void *pokemon=NULL;if(silph_callback_object(input,s->initializer))s->api.field_get(input,s->initializer_pokemon,&pokemon);color_enqueue(s,object,hundo(s,pokemon),false);
}
static void panel_update(shadowhook_cpu_context_t *registers,void *data){
    Visual *s=callback_visual();VISUAL_SCOPE(s);void *object=(void *)(uintptr_t)registers->regs[0],*pokemon=(void *)(uintptr_t)registers->regs[1];
    if(s!=data||!enabled(s,IV_NAMES)||!s->color_supported||!silph_callback_object(object,s->panel))return;color_enqueue(s,object,hundo(s,pokemon),true);
}
static void setup_colors(Visual *s,uint32_t offset){
    s->cell=klass(s,"holo-game.dll","Niantic.Holoholo.UI","PokemonInventoryCellView");s->panel=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Pokemon","PokemonInfoPanel");
    s->cell_name=field(s,s->cell,"nameText","UnityEngine.UI.Text");s->panel_name=field(s,s->panel,"nameText","UnityEngine.UI.Text");
    s->get_color=method(s,s->text_class,"get_color","UnityEngine.Color",NULL);s->set_color=method(s,s->text_class,"set_color","System.Void","UnityEngine.Color");
    const void *init=s->cell?s->api.method(s->cell,"Initialize",1):NULL;uint32_t impl=0;
    bool valid=init&&s->base.method_get_param_count(init)==1&&!(s->base.method_get_flags(init,&impl)&0x10u)&&type(s,s->base.method_get_return_type(init),"System.Void");
    s->initializer=valid?s->api.class_from_type(s->api.parameter(init,0)):NULL;
    if(s->initializer&&!s->api.value_type(s->initializer))s->initializer_pokemon=field(s,s->initializer,"PokemonProto","Holoholo.Rpc.PokemonProto");
    const void *update=method(s,s->panel,"DoUpdate","System.Void","Holoholo.Rpc.PokemonProto");
    if(!s->cell_name||!s->panel_name||!s->get_color||!s->set_color||!s->initializer_pokemon||!update||!valid)return;
    s->color_supported=1;if(observe(s,init,offset,cell_init)||observe(s,update,offset,panel_update))s->color_supported=0;
}
static int load_api(Visual *s,SilphManagedResolver resolver,void *context){
#define LOAD(member,name) do{void *p=resolver("il2cpp_" name,context);if(!p)return 1;memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(method,"class_get_method_from_name");
    LOAD(parameter,"method_get_param");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(field,"class_get_field_from_name");LOAD(field_flags,"field_get_flags");LOAD(field_type,"field_get_type");LOAD(field_get,"field_get_value");LOAD(assignable,"class_is_assignable_from");LOAD(class_from_type,"class_from_type");LOAD(value_type,"class_is_valuetype");
#undef LOAD
    return 0;
}
static void setup_iv(Visual *,uint32_t);
static void setup_trainer(Visual *,uint32_t);
static int open_visual(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,uint32_t flags,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address,bool deferred){
    if(!memory||bytes!=sizeof(Visual)||!base||!resolver||!allowed||!address||offset>120||offset%sizeof(void *)||atomic_load(&current))return 1;
    Visual *s=memory;memset(s,0,bytes);atomic_init(&s->owner,deferred?0:(unsigned)syscall(__NR_gettid));s->base=*base;s->context=context;s->allowed=allowed;s->allowed_context=allowed_context;s->address=address;s->requested=flags&(TRAINER|IV_NAMES);
    if(load_api(s,resolver,context))return 1;
    s->text_class=klass(s,"UnityEngine.UI.dll","UnityEngine.UI","Text");s->text_alive=field(s,s->text_class,"m_CachedPtr","System.IntPtr");
    s->get_text=method(s,s->text_class,"get_text","System.String",NULL);s->set_text=method(s,s->text_class,"set_text","System.Void","System.String");
    s->managed=calloc(1,silph_managed_size(VISUAL_REFS));if(!s->managed)return 1;
    if(!deferred&&prepare_managed(s)){free(s->managed);s->managed=NULL;return 1;}
    if(deferred){s->startup_allowed=allowed;s->startup_context=allowed_context;}
    atomic_store_explicit(&current,s,memory_order_release);
    if(flags&IV_NAMES)setup_iv(s,offset);
    if(flags&TRAINER)setup_trainer(s,offset);
    return 0;
}
int silph_visual_open(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,uint32_t flags,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address){
    return open_visual(memory,bytes,base,resolver,context,offset,flags,allowed,allowed_context,address,false);
}
int silph_visual_stage(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,uint32_t flags,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address){
    return open_visual(memory,bytes,base,resolver,context,offset,flags,allowed,allowed_context,address,true);
}
int silph_visual_adopt(void *memory,uint32_t flags,SilphVisualAllowed allowed,void *context,uint32_t offset){
    Visual *s=memory;if(!s||atomic_load(&current)!=s||!allowed||s->closed)return 1;
    Visual *bound=callback_visual();if(bound!=s)return 1;
    uint32_t add=(flags&(TRAINER|IV_NAMES))&~s->requested;s->requested|=add;
    if(add&IV_NAMES)setup_iv(s,offset);if(add&TRAINER)setup_trainer(s,offset);
    s->allowed=allowed;s->allowed_context=context;return 0;
}
int silph_visual_offset(const SilphManagedApi *base,SilphManagedResolver resolver,void *context,SilphVisualRead read,uintptr_t first,uintptr_t second,uint32_t *offset){
    if(!base||!resolver||!read||!offset||!first||!second)return 1;Visual s={.base=*base};if(load_api(&s,resolver,context))return 1;
    void *hud=klass(&s,"holo-game.dll","Niantic.Holoholo.Map.UI","PlayerHudGuiController");if(!hud)return 5;
    const void *a=method(&s,hud,"Initialize","System.Void",NULL),*b=method(&s,hud,"SetVisible","System.Void","System.Boolean");uintptr_t av[16],bv[16];
    if(!a||!b)return 5;if(read((uintptr_t)a,av,sizeof(av))||read((uintptr_t)b,bv,sizeof(bv)))return 5;
    for(unsigned i=0;i<16;i++)if(av[i]==first&&bv[i]==second){*offset=i*sizeof(uintptr_t);return 0;}return 1;
}
static void setup_iv(Visual *s,uint32_t offset){
        s->pokemon=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");const char *names[]={"individualAttack_","individualDefense_","individualStamina_"};
        for(unsigned i=0;i<3;i++)s->iv[i]=field(s,s->pokemon,names[i],"System.Int32");s->egg=field(s,s->pokemon,"isEgg_","System.Boolean");
        const void *getter=method(s,s->pokemon,"get_Nickname","System.String",NULL);
        if(!s->iv[0]||!s->iv[1]||!s->iv[2]||!s->egg||hook(s,getter,offset,(void *)nickname,(void **)&nickname_original))s->failed|=IV_NAMES;else s->available|=IV_NAMES;
    setup_colors(s,offset);
}
static void setup_trainer(Visual *s,uint32_t offset){
        s->text_class=klass(s,"UnityEngine.UI.dll","UnityEngine.UI","Text");s->text_alive=field(s,s->text_class,"m_CachedPtr","System.IntPtr");
        s->get_text=method(s,s->text_class,"get_text","System.String",NULL);s->set_text=method(s,s->text_class,"set_text","System.Void","System.String");
        s->hud=klass(s,"holo-game.dll","Niantic.Holoholo.Map.UI","PlayerHudGuiController");s->profile=klass(s,"holo-game.dll","Niantic.Holoholo.UI","PlayerProfilePage");s->friends=klass(s,"holo-game.dll","Niantic.Holoholo.Social.UI","FriendsListHeaderCellView");
        s->hud_fields[0]=field(s,s->hud,"playerName","UnityEngine.UI.Text");s->hud_fields[1]=field(s,s->hud,"playerNameShadow","UnityEngine.UI.Text");
        s->profile_field=field(s,s->profile,"playerNameText","UnityEngine.UI.Text");s->friends_field=field(s,s->friends,"playerName","UnityEngine.UI.Text");
        s->get_hud=method(s,klass(s,"holo-game.dll","Niantic.Holoholo.Map","MapExploreState"),"get_PlayerHud","Niantic.Holoholo.Map.UI.IPlayerHudGuiController",NULL);
        const void *methods[]={method(s,s->hud,"Initialize","System.Void",NULL),method(s,s->hud,"SetVisible","System.Void","System.Boolean"),method(s,s->hud,"coiw","System.Void","Niantic.Holoholo.PlayerService.PlayerInfo"),method(s,s->profile,"Reinitialize","System.Void",NULL),method(s,s->friends,"SetI18NText","System.Void",NULL)};
        bool valid=s->get_text&&s->set_text&&s->text_alive&&s->hud_fields[0]&&s->hud_fields[1]&&s->profile_field&&s->friends_field;
        for(unsigned i=0;i<5;i++)valid=valid&&methods[i];

        void *proxies[]={(void *)hud_init,(void *)hud_visible,(void *)hud_info,(void *)profile,(void *)friends};
        void **originals[]={(void **)&hud_init_original,(void **)&hud_visible_original,(void **)&hud_info_original,(void **)&profile_original,(void **)&friends_original};
        if(valid)s->available|=TRAINER;
        if(valid)for(unsigned i=0;i<5;i++)if(hook(s,methods[i],offset,proxies[i],originals[i])){valid=false;break;}
        if(!valid){s->available&=~TRAINER;s->failed|=TRAINER;}
}
int silph_visual_metrics(void *memory,SilphVisualMetrics *out){
    if(!memory||!out)return 1;Visual *s=memory;SilphManagedMetrics m={0};if(s->managed_ready&&silph_managed_metrics(s->managed,&m))return 1;
    *out=(SilphVisualMetrics){.available=s->available,.failed=s->failed,.iv_reads=s->iv_reads,.name_updates=s->name_updates,.created=m.refs_created,.released=m.refs_released,.remaining=m.live_refs};return 0;
}
int silph_visual_close(void *memory,SilphVisualMetrics *out){
    Visual *s=memory;if(!s||atomic_load(&s->owner)!=(unsigned)syscall(__NR_gettid)||s->depth)return 1;s->closed=1;int status=0;
    for(unsigned i=0;i<s->hooks_used;i++){Hook *h=&s->hooks[i];if(h->stub){int removed=h->observe?shadowhook_unintercept(h->stub):shadowhook_unhook(h->stub);if(removed||memcmp((const void *)h->address,h->bytes,16))status=1;else h->stub=NULL;}}
    if(status)return status;
    for(unsigned i=0;i<s->text_count;i++){
        TextRecord *r=&s->texts[i];void *text=NULL,*original=NULL,*label=NULL,*actual=NULL;
        if(r->text.serial&&!silph_managed_target(s->managed,r->text,&text)&&alive(s,text)&&!silph_managed_target(s->managed,s->label,&label)){
            if(r->original.serial)silph_managed_target(s->managed,r->original,&original);
            if(invoke(s,s->get_text,text,NULL,&actual))status=1;
            else if(actual==label){void *args[]={original};if(invoke(s,s->set_text,text,args,NULL))status=1;}
        }release(s,&r->text);release(s,&r->original);
    }
    for(unsigned i=0;i<s->color_count;i++){ColorRecord *r=&s->colors[i];void *text=NULL;TextColor actual={0};
        if(r->text.serial&&!silph_managed_target(s->managed,r->text,&text)&&alive(s,text)&&!color_read(s,text,&actual)&&same_color(actual,r->applied))if(color_write(s,text,r->before))status=1;release(s,&r->text);}
    for(unsigned i=0;i<64;i++)release(s,&s->pending_colors[i].object);
    for(unsigned i=0;i<128;i++)release(s,&s->labels[i]);release(s,&s->label);
    SilphManagedMetrics m={0};if(s->managed_ready){int close=silph_managed_close(s->managed,&m);if(close)status=close;else{free(s->managed);s->managed=NULL;}}
    if(out)*out=(SilphVisualMetrics){.available=s->available,.failed=s->failed,.iv_reads=s->iv_reads,.name_updates=s->name_updates,.created=m.refs_created,.released=m.refs_released,.remaining=m.live_refs};
    if(!s->managed_ready&&s->managed){free(s->managed);s->managed=NULL;}
    if(!status)current=NULL;return status;
}
