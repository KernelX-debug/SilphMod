// Read only the score attached to the retained response, on the Unity tick.
// No hooks, account requests, persistence of game objects or guessed offsets.
export class RewardDataError extends Error {}
type Obj = Il2Cpp.Object;
function field(value:Obj,name:string):any {
  const found=value.tryField(name);
  if(!found || found.isStatic || found.offset<Il2Cpp.Object.headerSize)
    throw new RewardDataError('reward_field_unavailable:'+name);
  return found.value;
}
function obj(value:any,name:string):Obj {
  if(!(value instanceof Il2Cpp.Object)||value.isNull())throw new RewardDataError('reward_object_missing:'+name);
  if(value.class.fullName!==name)throw new RewardDataError('reward_class_mismatch:'+name);
  return value;
}
function integer(value:any,name:string):number {
  if(typeof value!=='number'||!Number.isSafeInteger(value)||value<0)
    throw new RewardDataError('invalid_reward_integer:'+name);
  return value;
}
function list(value:any,itemType:string):{value:Obj;length:number;item:{invoke(...args:any[]):any}} {
  if(!(value instanceof Il2Cpp.Object)||value.isNull())throw new RewardDataError('reward_list_missing');
  const count=value.tryMethod<number>('get_Count',0),item=value.tryMethod('get_Item',1);
  if(!count || !item || count.virtualAddress.isNull() || item.virtualAddress.isNull()
    || count.returnType.name!=='System.Int32' || item.parameters[0].type.name!=='System.Int32'
    || item.returnType.name!==itemType)throw new RewardDataError('reward_list_signature_mismatch');
  const length=integer(count.invoke(),'list_count');
  if(length>64)throw new RewardDataError('reward_list_too_large');
  return {value,length,item};
}
function intList(value:any):number[] {
  const sequence=list(value,'System.Int32'),result:number[]=[];
  for(let i=0;i<sequence.length;i++)result.push(integer(sequence.item.invoke(i),'score_component'));
  return result;
}
export function captureRewardData(result:Obj) {
  obj(result,'Holoholo.Rpc.CatchPokemonOutProto');
  const scores=obj(field(result,'Scores'),'Holoholo.Rpc.CaptureScoreProto');
  const temp=field(scores,'TempEvoScoreInfo');
  let tempEvo=0;
  if(!(temp instanceof Il2Cpp.Object))throw new RewardDataError('invalid_temp_evo_score');
  if(!temp.isNull()) {
    if(temp.class.name!=='TempEvoScoreInfo')throw new RewardDataError('invalid_temp_evo_class');
    tempEvo=integer(field(temp,'ExperienceFromActiveTempEvo'),'temp_evo');
  }
  return {exp:intList(field(scores,'Exp')),stardust:intList(field(scores,'Stardust')),
    mega:integer(field(scores,'ExperienceFromActiveMega'),'mega'),tempEvo};
}
function loot(value:any):{count:number;experience:boolean;stardust:boolean}[] {
  if(!(value instanceof Il2Cpp.Object))throw new RewardDataError('invalid_loot');
  if(value.isNull())return [];
  const source=obj(value,'Holoholo.Rpc.LootProto');
  const sequence=list(field(source,'LootItem'),'Holoholo.Rpc.LootItemProto'),items=[];
  for(let i=0;i<sequence.length;i++) {
    const item=obj(sequence.item.invoke(i),'Holoholo.Rpc.LootItemProto');
    const kind=item.tryMethod('get_TypeCase',0);
    if(!kind || kind.virtualAddress.isNull() || !kind.returnType.class.isEnum)
      throw new RewardDataError('loot_kind_signature_mismatch');
    const raw=kind.invoke();
    // Enum values are read through their reflected value__, never a pointer offset.
    const type=integer(raw instanceof Il2Cpp.ValueType?raw.field('value__').value:raw,'loot_kind');
    if(type>16)throw new RewardDataError('unknown_loot_kind');
    items.push({count:integer(field(item,'Count'),'loot_count'),experience:type===6,stardust:type===2});
  }
  return items;
}
export function spinRewardData(result:Obj) {
  obj(result,'Holoholo.Rpc.FortSearchOutProto');
  return {xpAwarded:integer(field(result,'XpAwarded'),'spin_xp'),
    loot:loot(field(result,'Loot')),bonusLoot:loot(field(result,'BonusLoot'))};
}
