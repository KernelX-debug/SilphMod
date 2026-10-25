#include "gift-actions.h"
#include "response-rewards.h"
#include <stdlib.h>
#include <string.h>
#define OPEN_GIFTS 16384u
#define SEND_GIFTS 32768u
typedef struct {
    void *read,*managed,*game;SilphScheduleIdentity identity;SilphGiftMetrics metrics;
    SilphManagedRef friends,rpc,inventory,handler,promise,iterator,player,nia;
    uint32_t flags,opened,phase,visited,used_open,used_send,full,raw;uint64_t due,started,open_after,active_gift;
    uint64_t open_ids[2048],send_ids[2048];char friend_id[257];
} Gifts;
size_t silph_gift_size(void){return sizeof(Gifts);}
static int drop(Gifts *s,SilphManagedRef *r){return silph_reward_drop(s->managed,r);}
static int scalar(Gifts *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,kind,&v);if(!r)*out=v.bits;return r;}
static int ref_field(Gifts *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;return r;}
static int ref_call(Gifts *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_call(s->read,ref,name,SILPH_READ_REFERENCE,NULL,0,&v);if(!r)*out=v.reference;return r;}
static int text(Gifts *s,SilphManagedRef ref,char *out){uint32_t n=0;int r=silph_read_utf8(s->read,ref,out,257,&n);if(!r&&(!n||n>256))r=SILPH_MANAGED_CONTRACT;return r;}
static bool seen(const uint64_t *ids,unsigned n,uint64_t id){for(unsigned i=0;i<n;i++)if(ids[i]==id)return true;return false;}
static void forget(uint64_t *ids,uint32_t *n,uint64_t id){for(unsigned i=0;i<*n;i++)if(ids[i]==id){ids[i]=ids[--*n];ids[*n]=0;break;}}
static void dispose(Gifts *s,SilphManagedRef *ref){if(ref->serial){SilphReadValue v={0};silph_gift_enumerate(s->read,*ref,3,&v);drop(s,ref);}}
static void next(Gifts *s,uint64_t now){drop(s,&s->player);drop(s,&s->nia);s->friend_id[0]=0;s->active_gift=0;s->metrics.state=2;s->due=now+UINT64_C(200000000);}
static int send(Gifts *s,const SilphSchedulePermit *p,uint32_t op,SilphManagedRef ref,SilphReadArgument *args,uint32_t n,uint64_t now){
    SilphReadValue v={0};s->metrics.operation=op;s->raw=op==4&&s->full;
    int r=s->raw?silph_gift_open_stardust(s->read,p,s->handler,s->player,args[0].value.bits,&v):silph_gift_invoke(s->read,p,op,ref,args,n,&v);
    if(!r&&!v.reference.serial)r=SILPH_MANAGED_CONTRACT;
    if(!r){s->promise=v.reference;s->phase=op;s->started=now;s->metrics.requests++;s->metrics.state=3;s->due=now+UINT64_C(100000000);}
    else drop(s,&v.reference);return r;
}
static int iterator(Gifts *s,SilphManagedRef list,SilphManagedRef *out){
    SilphReadValue value={0};int r=silph_read_call(s->read,list,"GetEnumerator",SILPH_READ_BOXED,NULL,0,&value);
    if(r==SILPH_MANAGED_CONTRACT)r=silph_read_call(s->read,list,"GetEnumerator",SILPH_READ_REFERENCE,NULL,0,&value);
    if(r==SILPH_MANAGED_CONTRACT)r=silph_gift_enumerate(s->read,list,4,&value);
    if(!r)*out=value.reference;return r;
}
static int advance(Gifts *s,SilphManagedRef it,bool *more,SilphManagedRef *current){
    s->metrics.operation=67;SilphReadValue v={0};int r=silph_gift_enumerate(s->read,it,1,&v);*more=!r&&v.bits;
    if(!r&&*more){s->metrics.operation=68;r=silph_gift_enumerate(s->read,it,2,&v);if(!r)*current=v.reference;}return r;
}
static int available_gift(Gifts *s,const SilphSchedulePermit *p,bool outgoing,uint64_t *id){
    *id=0;SilphManagedRef list={0},it={0},gift={0};int r=0;
    s->metrics.operation=outgoing?60:61;if(outgoing)r=ref_call(s,s->inventory,"get_SendableGiftsInInventory",&list);
    else{SilphReadArgument a={.kind=SILPH_READ_REFERENCE,.value.reference=s->player};SilphReadValue v={0};r=silph_gift_invoke(s->read,p,6,s->friends,&a,1,&v);if(!r)list=v.reference;}
    if(!r&&list.serial){s->metrics.operation=62;r=iterator(s,list,&it);}
    for(unsigned i=0;!r&&it.serial&&i<128;i++){
        s->metrics.operation=63;bool more=false;r=advance(s,it,&more,&gift);if(r||!more)break;
        s->metrics.operation=64;r=silph_read_is_type(s->read,gift,"Holoholo.Rpc",outgoing?"GiftBoxProto":"GiftBoxDetailsProto");uint64_t value=0;
        if(!r){s->metrics.operation=65;r=scalar(s,gift,"GiftboxId",SILPH_READ_U64,&value);}
        if(!r&&value&&!seen(outgoing?s->send_ids:s->open_ids,outgoing?s->used_send:s->used_open,value)){
            SilphManagedRef sender={0};char sender_id[257]={0};
            if(!outgoing){s->metrics.operation=66;r=ref_field(s,gift,"SenderId",&sender);if(!r)r=text(s,sender,sender_id);drop(s,&sender);}
            if(!r&&(outgoing||!strcmp(sender_id,s->friend_id)))*id=value;
        }
        drop(s,&gift);if(*id)break;
    }
    drop(s,&gift);dispose(s,&it);drop(s,&list);return r;
}
static int check_send(Gifts *s,const SilphSchedulePermit *p,uint64_t now){
    if(!(s->flags&SEND_GIFTS)){next(s,now);return 0;}
    uint64_t gift=0;int r=available_gift(s,p,true,&gift);if(r)return r;
    if(!gift||s->used_send==2048){next(s,now);return 0;}
    s->active_gift=gift;SilphReadArgument a={.kind=SILPH_READ_REFERENCE,.value.reference=s->player};return send(s,p,3,s->rpc,&a,1,now);
}
static int detail_ready(Gifts *s,const SilphSchedulePermit *p,uint64_t now){
    uint64_t gift=0;int r=available_gift(s,p,false,&gift);if(r)return r;
    if(gift&&s->used_open<2048){
        s->open_ids[s->used_open++]=gift;s->active_gift=gift;
        SilphReadArgument a[]={{.kind=SILPH_READ_U64,.value.bits=gift},{.kind=SILPH_READ_REFERENCE,.value.reference=s->player},{.kind=SILPH_READ_REFERENCE,.value.reference=s->nia}};
        return send(s,p,4,s->rpc,a,3,now);
    }
    return check_send(s,p,now);
}
static int completed(Gifts *s,const SilphSchedulePermit *p,SilphManagedRef result,uint64_t now){
    uint32_t op=s->phase;int r=0;
    if(op==1){s->metrics.operation=14;SilphManagedRef values={0};r=ref_call(s,result,"get_Values",&values);if(!r){s->metrics.operation=15;r=iterator(s,values,&s->iterator);}drop(s,&values);if(!r){s->visited=0;s->phase=0;s->metrics.state=2;}return r;}
    if(op==2)return detail_ready(s,p,now);
    const char *name=op==3?"CheckSendGiftOutProto":op==4?"OpenGiftOutProto":"SendGiftOutProto";
    r=silph_read_is_type(s->read,result,"Holoholo.Rpc",name);uint64_t code=0;if(!r)r=scalar(s,result,"Result",SILPH_READ_ENUM32,&code);s->metrics.result=(uint32_t)code;
    if(r||code>12)return r?r:SILPH_MANAGED_CONTRACT;
    if(op==3&&code==1){
        uint64_t fresh=0;r=available_gift(s,p,true,&fresh);
        if(!r&&fresh&&s->used_send<2048){s->active_gift=fresh;s->send_ids[s->used_send++]=fresh;
            SilphReadArgument a[]={{.kind=SILPH_READ_U64,.value.bits=fresh},{.kind=SILPH_READ_REFERENCE,.value.reference=s->player},{.kind=SILPH_READ_REFERENCE,.value.reference=s->nia}};
            return send(s,p,5,s->rpc,a,3,now);
        }
    }
    if(op==4){if(code==1){s->metrics.opened++;uint64_t xp=0,dust=0;int rewards=silph_reward_loot(s->read,s->managed,result,"Items",&xp,&dust);if(!rewards){s->metrics.xp+=xp;s->metrics.stardust+=dust;}
            if(s->raw){s->metrics.converted++;SilphReadArgument a[]={{.kind=SILPH_READ_REFERENCE,.value.reference=s->player},{.kind=SILPH_READ_U64,.value.bits=s->active_gift}};SilphReadValue ignored={0};int update=silph_gift_invoke(s->read,p,7,s->friends,a,2,&ignored);if(update)s->metrics.error=700+(uint32_t)update;}}
        else{s->metrics.skipped++;if(code==3||code==4){forget(s->open_ids,&s->used_open,s->active_gift);s->open_after=now+(code==4?UINT64_C(300000000000):UINT64_C(60000000000));}}
        return check_send(s,p,now);
    }
    if(op==5&&code==1)s->metrics.sent++;else{s->metrics.skipped++;if(op==5&&(code==3||code==5||code==6))forget(s->send_ids,&s->used_send,s->active_gift);}next(s,now);return r;
}
static int poll(Gifts *s,const SilphSchedulePermit *p,uint64_t now){
    s->metrics.operation=s->phase*10+1;uint64_t complete=0,error=0;int r=scalar(s,s->promise,"completeCalled",SILPH_READ_BOOL,&complete);
    if(!r){s->metrics.operation=s->phase*10+2;r=scalar(s,s->promise,"errorCalled",SILPH_READ_BOOL,&error);}
    if(!r&&s->raw){uint64_t status_error=0;r=scalar(s,s->promise,"statusErrorCalled",SILPH_READ_BOOL,&status_error);if(!r&&status_error)error=1;}
    if(!r&&!error&&!complete&&now-s->started<UINT64_C(18000000000))return 0;
    if(!r&&(error||!complete))r=SILPH_MANAGED_NOT_READY;
    SilphManagedRef result={0};if(!r){s->metrics.operation=s->phase*10+3;r=ref_field(s,s->promise,"completedValue",&result);}if(!r&&!result.serial)r=SILPH_MANAGED_CONTRACT;
    drop(s,&s->promise);if(!r)r=completed(s,p,result,now);drop(s,&result);return r;
}
int silph_gift_open(void *memory,size_t bytes,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,uint32_t flags){
    if(!memory||bytes!=sizeof(Gifts)||!read||!managed||!game||!identity||(!identity->account_hi&&!identity->account_lo)||!(flags&(OPEN_GIFTS|SEND_GIFTS)))return SILPH_MANAGED_ARGUMENT;
    Gifts *s=memory;if(s->opened)return SILPH_MANAGED_STATE;memset(s,0,bytes);s->read=read;s->managed=managed;s->game=game;s->identity=*identity;s->flags=flags;s->opened=1;
    int r=silph_game_read_gift_service(game,0,&s->friends);if(!r)r=silph_game_read_gift_service(game,1,&s->rpc);
    if(!r&&(flags&OPEN_GIFTS))r=silph_game_read_rpc_handler(game,&s->handler);
    if(!r&&(flags&SEND_GIFTS))r=silph_game_read_gift_service(game,2,&s->inventory);
    s->metrics.error=(uint32_t)r;s->metrics.state=r?5:1;return 0;
}
int silph_gift_tick(void *memory,const SilphSchedulePermit *p,const SilphGameSnapshot *snapshot,uint64_t now,SilphGiftMetrics *out){
    Gifts *s=memory;if(!s||!s->opened||!snapshot||!out||!silph_schedule_permitted(p,true))return SILPH_MANAGED_STATE;
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo)return SILPH_MANAGED_STALE;
    if(s->metrics.state==5||now<s->due){*out=s->metrics;return 0;}s->full=snapshot->max_items&&snapshot->item_count>=snapshot->max_items;s->due=now+UINT64_C(100000000);int r=0;
    if(s->promise.serial)r=poll(s,p,now);
    else if(s->metrics.state==1)r=send(s,p,1,s->friends,NULL,0,now);
    else if(s->metrics.state==2){
        s->metrics.operation=20;bool more=false;SilphManagedRef friend={0};r=advance(s,s->iterator,&more,&friend);
        if(!r&&more&&++s->visited<=2000){
            s->metrics.operation=21;r=silph_read_is_type(s->read,friend,"Niantic.Platform.Internal.Protos","FriendProto");
            if(r==SILPH_MANAGED_CONTRACT)r=silph_read_is_type(s->read,friend,"","FriendProto");
            if(!r){s->metrics.operation=22;r=ref_call(s,friend,"get_PlayerId",&s->player);}if(!r){s->metrics.operation=23;r=ref_call(s,friend,"get_NiaAccountId",&s->nia);}if(!r){s->metrics.operation=24;r=text(s,s->player,s->friend_id);}
            if(!r&&(s->flags&OPEN_GIFTS)&&now>=s->open_after&&s->used_open<2048){SilphReadArgument a[]={{.kind=SILPH_READ_REFERENCE,.value.reference=s->player},{.kind=SILPH_READ_REFERENCE,.value.reference=s->nia}};r=send(s,p,2,s->friends,a,2,now);}
            else if(!r)r=check_send(s,p,now);
        }else if(!r){dispose(s,&s->iterator);next(s,now);s->metrics.state=1;s->due=now+UINT64_C(120000000000);}
        drop(s,&friend);
    }
    if(r){uint32_t phase=s->phase;bool changed=s->metrics.state==2&&(s->metrics.operation==67||s->metrics.operation==68);s->metrics.error=s->metrics.operation*100+(uint32_t)r;s->metrics.skipped++;drop(s,&s->promise);next(s,now);
        if(phase==1||changed||!s->iterator.serial){dispose(s,&s->iterator);s->metrics.state=1;s->due=now+UINT64_C(30000000000);}}
    *out=s->metrics;return 0;
}
int silph_gift_close(void *memory){
    Gifts *s=memory;if(!s||!s->opened)return 0;dispose(s,&s->iterator);SilphManagedRef *refs[]={&s->promise,&s->player,&s->nia,&s->inventory,&s->handler,&s->rpc,&s->friends};int r=0;
    for(unsigned i=0;i<7;i++){int freed=drop(s,refs[i]);if(!r)r=freed;}if(!r)memset(s,0,sizeof(*s));return r;
}
