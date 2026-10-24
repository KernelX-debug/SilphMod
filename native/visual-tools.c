#define _GNU_SOURCE
#include "visual-tools.h"
#include <shadowhook.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#define TRAINER 128u
#define IV_NAMES 256u
typedef struct {
    const void **(*assemblies)(void *,size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *);
    const void *(*method)(void *,const char *,int);
    const void *(*parameter)(const void *,uint32_t);
    const char *(*type_name)(const void *);
    void (*free)(void *);
    const void *(*field)(void *,const char *);
    uint32_t (*field_flags)(const void *);
    const void *(*field_type)(const void *);
    void (*field_get)(void *,const void *,void *);
} Api;
typedef struct {void *stub;uintptr_t address;unsigned char bytes[16];} Hook;
typedef struct {SilphManagedRef text,original;} TextRecord;
typedef struct {
    unsigned owner,closed;uint32_t requested,available,failed;
    SilphManagedApi base;Api api;void *managed,*context,*allowed_context;
    SilphVisualAllowed allowed;SilphVisualAddress address;
    void *pokemon,*text_class,*hud,*profile,*friends;
    const void *iv[3],*egg,*text_alive,*hud_fields[2],*profile_field,*friends_field;
    const void *get_text,*set_text,*get_hud;unsigned hud_primed;
    Hook hooks[6];unsigned hooks_used;
    SilphManagedRef label,labels[128];unsigned keys[128],next_label;
    TextRecord texts[48];unsigned text_count;
    uint64_t iv_reads,name_updates;
} Visual;
static Visual *current;
static void *(*nickname_original)(void *,const void *);
static void (*hud_init_original)(void *,const void *),(*hud_visible_original)(void *,bool,const void *);
static void (*hud_info_original)(void *,void *,const void *),(*profile_original)(void *,const void *),(*friends_original)(void *,const void *);
int silph_visual_iv_label(int a,int d,int s,bool egg,char *out,size_t capacity){
    if(!out||capacity<22||egg||a<0||a>15||d<0||d>15||s<0||s>15)return 1;
    int n=snprintf(out,capacity,"%d%% %d/%d/%d",(a+d+s)*100/45,a,d,s);return n<0||(size_t)n>=capacity;
}
size_t silph_visual_size(void){return sizeof(Visual);}
static bool enabled(Visual *s,uint32_t bit){return s&&!s->closed&&(s->available&bit)&&s->owner==(unsigned)syscall(__NR_gettid)&&s->allowed(s->allowed_context);}
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
    if(!method||s->hooks_used>=6)return 1;uintptr_t address=0;memcpy(&address,(const char *)method+offset,sizeof(address));
    if((address&3)||!s->address(address,s->context))return 1;
    Hook *h=&s->hooks[s->hooks_used];h->address=address;memcpy(h->bytes,(const void *)address,16);
    h->stub=shadowhook_hook_func_addr((void *)address,proxy,original);if(!h->stub)return 1;s->hooks_used++;return 0;
}
static int invoke(Visual *s,const void *method,void *object,void **args,void **out){
    void *exception=NULL,*result=s->base.runtime_invoke(method,object,args,&exception);if(exception)return 1;if(out)*out=result;return 0;
}
static bool alive(Visual *s,void *text){
    if(!text||s->base.object_get_class(text)!=s->text_class)return false;uintptr_t ptr=0;s->api.field_get(text,s->text_alive,&ptr);return ptr!=0;
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
    if(!enabled(s,TRAINER)||!object||s->base.object_get_class(object)!=expected)return;
    void *text=NULL;s->api.field_get(object,a,&text);int status=mask_text(s,text);
    if(!status&&b){text=NULL;s->api.field_get(object,b,&text);status=mask_text(s,text);}
    if(status){s->available&=~TRAINER;s->failed|=TRAINER;}
}
static void hud_init(void *o,const void *m){hud_init_original(o,m);Visual *s=current;if(s)mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);}
static void hud_visible(void *o,bool v,const void *m){hud_visible_original(o,v,m);Visual *s=current;if(s&&v)mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);}
static void hud_info(void *o,void *v,const void *m){hud_info_original(o,v,m);Visual *s=current;if(s)mask(s,o,s->hud,s->hud_fields[0],s->hud_fields[1]);}
static void profile(void *o,const void *m){profile_original(o,m);Visual *s=current;if(s)mask(s,o,s->profile,s->profile_field,NULL);}
static void friends(void *o,const void *m){friends_original(o,m);Visual *s=current;if(s)mask(s,o,s->friends,s->friends_field,NULL);}
void silph_visual_prime(void *memory,void *map){
    Visual *s=memory;if(!enabled(s,TRAINER)||s->hud_primed||!map||!s->get_hud)return;
    void *hud=NULL;if(invoke(s,s->get_hud,map,NULL,&hud)||!hud)return;
    mask(s,hud,s->hud,s->hud_fields[0],s->hud_fields[1]);s->hud_primed=1;
}
static void *nickname(void *o,const void *m){
    void *original=nickname_original(o,m);Visual *s=current;if(!enabled(s,IV_NAMES)||!o||s->base.object_get_class(o)!=s->pokemon)return original;
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
int silph_visual_open(void *memory,size_t bytes,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,uint32_t flags,SilphVisualAllowed allowed,void *allowed_context,SilphVisualAddress address){
    if(!memory||bytes!=sizeof(Visual)||!base||!resolver||!allowed||!address||offset>120||offset%sizeof(void *)||current)return 1;
    Visual *s=memory;memset(s,0,bytes);s->base=*base;s->context=context;s->allowed=allowed;s->allowed_context=allowed_context;s->address=address;s->owner=(unsigned)syscall(__NR_gettid);s->requested=flags&(TRAINER|IV_NAMES);
#define LOAD(member,name) do{void *p=resolver("il2cpp_" name,context);if(!p)return 1;memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(method,"class_get_method_from_name");
    LOAD(parameter,"method_get_param");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(field,"class_get_field_from_name");LOAD(field_flags,"field_get_flags");LOAD(field_type,"field_get_type");LOAD(field_get,"field_get_value");
#undef LOAD
    size_t size=silph_managed_size(256);s->managed=calloc(1,size);if(!s->managed)return 1;
    int status=silph_managed_open(s->managed,size,base,256,64);if(status){free(s->managed);s->managed=NULL;return status;}current=s;
    if(flags&IV_NAMES){
        s->pokemon=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");const char *names[]={"individualAttack_","individualDefense_","individualStamina_"};
        for(unsigned i=0;i<3;i++)s->iv[i]=field(s,s->pokemon,names[i],"System.Int32");s->egg=field(s,s->pokemon,"isEgg_","System.Boolean");
        const void *getter=method(s,s->pokemon,"get_Nickname","System.String",NULL);
        if(!s->iv[0]||!s->iv[1]||!s->iv[2]||!s->egg||hook(s,getter,offset,(void *)nickname,(void **)&nickname_original))s->failed|=IV_NAMES;else s->available|=IV_NAMES;
    }
    if(flags&TRAINER){
        s->text_class=klass(s,"UnityEngine.UI.dll","UnityEngine.UI","Text");s->text_alive=field(s,s->text_class,"m_CachedPtr","System.IntPtr");
        s->get_text=method(s,s->text_class,"get_text","System.String",NULL);s->set_text=method(s,s->text_class,"set_text","System.Void","System.String");
        s->hud=klass(s,"holo-game.dll","Niantic.Holoholo.Map.UI","PlayerHudGuiController");s->profile=klass(s,"holo-game.dll","Niantic.Holoholo.UI","PlayerProfilePage");s->friends=klass(s,"holo-game.dll","Niantic.Holoholo.Social.UI","FriendsListHeaderCellView");
        s->hud_fields[0]=field(s,s->hud,"playerName","UnityEngine.UI.Text");s->hud_fields[1]=field(s,s->hud,"playerNameShadow","UnityEngine.UI.Text");
        s->profile_field=field(s,s->profile,"playerNameText","UnityEngine.UI.Text");s->friends_field=field(s,s->friends,"playerName","UnityEngine.UI.Text");
        s->get_hud=method(s,klass(s,"holo-game.dll","Niantic.Holoholo.Map","MapExploreState"),"get_PlayerHud","Niantic.Holoholo.Map.UI.IPlayerHudGuiController",NULL);
        const void *methods[]={method(s,s->hud,"Initialize","System.Void",NULL),method(s,s->hud,"SetVisible","System.Void","System.Boolean"),method(s,s->hud,"coiw","System.Void","Niantic.Holoholo.PlayerService.PlayerInfo"),method(s,s->profile,"Reinitialize","System.Void",NULL),method(s,s->friends,"SetI18NText","System.Void",NULL)};
        bool valid=s->get_text&&s->set_text&&s->text_alive&&s->hud_fields[0]&&s->hud_fields[1]&&s->profile_field&&s->friends_field;
        for(unsigned i=0;i<5;i++)valid=valid&&methods[i];
        if(valid&&silph_managed_ascii(s->managed,"SilphMod",8,&s->label))valid=false;
        void *proxies[]={(void *)hud_init,(void *)hud_visible,(void *)hud_info,(void *)profile,(void *)friends};
        void **originals[]={(void **)&hud_init_original,(void **)&hud_visible_original,(void **)&hud_info_original,(void **)&profile_original,(void **)&friends_original};
        if(valid)for(unsigned i=0;i<5;i++)if(hook(s,methods[i],offset,proxies[i],originals[i])){valid=false;break;}
        if(valid)s->available|=TRAINER;else s->failed|=TRAINER;
    }
    return 0;
}
int silph_visual_metrics(void *memory,SilphVisualMetrics *out){
    if(!memory||!out)return 1;Visual *s=memory;SilphManagedMetrics m={0};if(s->managed&&silph_managed_metrics(s->managed,&m))return 1;
    *out=(SilphVisualMetrics){.available=s->available,.failed=s->failed,.iv_reads=s->iv_reads,.name_updates=s->name_updates,.created=m.refs_created,.released=m.refs_released,.remaining=m.live_refs};return 0;
}
int silph_visual_close(void *memory,SilphVisualMetrics *out){
    Visual *s=memory;if(!s||s->owner!=(unsigned)syscall(__NR_gettid))return 1;s->closed=1;int status=0;
    for(unsigned i=0;i<s->hooks_used;i++){Hook *h=&s->hooks[i];if(h->stub){if(shadowhook_unhook(h->stub)||memcmp((const void *)h->address,h->bytes,16))status=1;else h->stub=NULL;}}
    if(status)return status;
    for(unsigned i=0;i<s->text_count;i++){
        TextRecord *r=&s->texts[i];void *text=NULL,*original=NULL,*label=NULL,*actual=NULL;
        if(r->text.serial&&!silph_managed_target(s->managed,r->text,&text)&&alive(s,text)&&!silph_managed_target(s->managed,s->label,&label)){
            if(r->original.serial)silph_managed_target(s->managed,r->original,&original);
            if(invoke(s,s->get_text,text,NULL,&actual))status=1;
            else if(actual==label){void *args[]={original};if(invoke(s,s->set_text,text,args,NULL))status=1;}
        }release(s,&r->text);release(s,&r->original);
    }
    for(unsigned i=0;i<128;i++)release(s,&s->labels[i]);release(s,&s->label);
    SilphManagedMetrics m={0};if(s->managed){int close=silph_managed_close(s->managed,&m);if(close)status=close;else{free(s->managed);s->managed=NULL;}}
    if(out)*out=(SilphVisualMetrics){.available=s->available,.failed=s->failed,.iv_reads=s->iv_reads,.name_updates=s->name_updates,.created=m.refs_created,.released=m.refs_released,.remaining=m.live_refs};
    if(!status)current=NULL;return status;
}
