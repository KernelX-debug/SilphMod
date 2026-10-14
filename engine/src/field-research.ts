export class ResearchProfileError extends Error {}
type Obj = Il2Cpp.Object;
type Runtime = {
  klass:(assembly:string,name:string)=>Il2Cpp.Class;
  service:(name:string)=>Obj;
  factory:(type:Il2Cpp.Class)=>Obj;
  retain:(value:Obj,label:string)=>{free():void};
  enumerate:(value:Obj,limit:number)=>Obj[];
  number:(value:any)=>number;
};
function method(owner:Obj|Il2Cpp.Class,name:string,parameters:string[],result:string) {
  const value=owner.tryMethod(name,parameters.length);
  const normalize=(name:string)=>name.replace(/[+/]/g,'.').replace(/`\d+/g,'');
  if(!value || value.virtualAddress.isNull() || value.parameters.length!==parameters.length
    || value.parameters.some((p,i)=>normalize(p.type.name)!==normalize(parameters[i])) || normalize(value.returnType.name)!==normalize(result))
    throw new ResearchProfileError('research_method_mismatch:'+JSON.stringify({name,expectedParameters:parameters,
      actualParameters:value?.parameters.map(p=>p.type.name)??null,expectedReturn:result,actualReturn:value?.returnType.name??null}));
  return value;
}
function fields(type:Il2Cpp.Class,names:string[]) {
  for(const name of names) {
    const value=type.tryField(name);
    if(!value || value.isStatic || value.offset<Il2Cpp.Object.headerSize)
      throw new ResearchProfileError('research_field_mismatch:'+type.fullName+'.'+name);
  }
}
function obj(value:any,expected?:string):Obj {
  if(!(value instanceof Il2Cpp.Object)||value.isNull()||expected&&value.class.fullName!==expected)
    throw new ResearchProfileError('research_object_mismatch:'+expected);
  return value;
}
function text(value:any):string {
  if(!(value instanceof Il2Cpp.String) || value.content===null)throw new ResearchProfileError('research_string_missing');
  return value.content;
}
export class FieldResearchAdapter {
  private quests:Obj;
  private rpc:Obj;
  private canRedeem:Il2Cpp.Method;
  private nullableNow:Il2Cpp.Class;
  private nullableChoice:Il2Cpp.Class;
  private dangling:Il2Cpp.Method;
  private complete:Il2Cpp.Method;
  private remove:Il2Cpp.Method;
  private begin:Il2Cpp.Method;
  private factory:Obj|null=null;
  constructor(private runtime:Runtime) {
    const {klass,service}=runtime;
    const quest=klass('holo-protos','Holoholo.Rpc.QuestProto');
    fields(quest,['QuestId','QuestContext','QuestType','Status','QuestRewards','TemplateId','Goal']);
    for(const [type,names] of [
      ['CompleteQuestOutProto',['Status','Rewards']],['RemoveQuestOutProto',['Status']],
      ['QuestPokemonEncounterProto',['QuestId','Pokemon','EncounterType','PokeBallOverride']],
      ['QuestEncounterOutProto',['Result','Pokemon','ActiveItem']],['QuestRewardProto',['Type']],
      ['QuestGoalProto',['Condition']],['QuestConditionProto',['Type']],
    ] as [string,string[]][])fields(klass('holo-protos','Holoholo.Rpc.'+type),names);
    this.quests=service('Niantic.Holoholo.Quests.IQuestService');
    this.rpc=service('Niantic.Holoholo.Quests.IQuestRpcService');
    const canRedeem=klass('holo-game','Niantic.Holoholo.Quests.QuestProtoExt').tryMethod('CanRedeem',2);
    const complete=this.rpc.tryMethod('CompleteQuest',4);
    if(!canRedeem || canRedeem.parameters.length!==2 || !complete || complete.parameters.length!==4)
      throw new ResearchProfileError('research_entry_signature');
    this.canRedeem=canRedeem;this.complete=complete;
    this.nullableNow=this.canRedeem.parameters[1].type.class;
    this.nullableChoice=this.complete.parameters[2].type.class;
    for(const [type,primitive] of [[this.nullableNow,'System.Int64'],[this.nullableChoice,'System.Int32']] as const) {
      const hasValue=type.tryField('hasValue'),value=type.tryField('value'),header=Il2Cpp.Object.headerSize;
      if(!type.isValueType || hasValue?.type.name!=='System.Boolean' || value?.type.name!==primitive
        || hasValue.isStatic || value.isStatic || hasValue.offset<header || value.offset<header
        || hasValue.offset+1>header+type.valueTypeSize || value.offset+(primitive==='System.Int64'?8:4)>header+type.valueTypeSize
        || !Number.isInteger(type.valueTypeSize)
        || type.valueTypeSize<8 || type.valueTypeSize>32)
        throw new ResearchProfileError('research_nullable_mismatch:'+primitive);
    }
    method(klass('holo-game','Niantic.Holoholo.Quests.QuestProtoExt'),'CanRedeem',
      ['Holoholo.Rpc.QuestProto',this.canRedeem.parameters[1].type.name],'System.Boolean');
    if(!this.canRedeem.isStatic)throw new ResearchProfileError('research_redeem_not_static');
    method(this.rpc,'CompleteQuest',['System.String','System.String',this.complete.parameters[2].type.name,'System.UInt64'],
      'Niantic.Promises.IPromise<Holoholo.Rpc.CompleteQuestOutProto>');
    this.remove=method(this.rpc,'RemoveQuest',['System.String'],'Niantic.Promises.IPromise<Holoholo.Rpc.RemoveQuestOutProto>');
    this.begin=method(this.rpc,'BeginQuestEncounter',['Holoholo.Rpc.QuestPokemonEncounterProto'],
      'Niantic.Promises.IPromise<Holoholo.Rpc.QuestEncounterOutProto>');
    method(this.quests,'GetQuestDataOfType',['Holoholo.Rpc.QuestProto.Types.Context'],
      'System.Collections.Generic.IList<Holoholo.Rpc.QuestProto>');
    this.dangling=this.quests.method('GetDanglingEncounterOfType',2).overload('Holoholo.Rpc.EncounterType','System.String');
    if(this.dangling.virtualAddress.isNull() || this.dangling.returnType.name!=='Holoholo.Rpc.QuestPokemonEncounterProto')
      throw new ResearchProfileError('research_dangling_signature');
    method(this.quests,'get_ExclusiveQuestCompletionInProgress',[],'System.Boolean');
    method(this.quests,'CanReceiveReward',['Holoholo.Rpc.QuestRewardProto'],'System.Boolean');
  }
  private nullable<T>(type:Il2Cpp.Class,call:(value:Il2Cpp.ValueType)=>T):T {
    const storage=Memory.alloc(type.valueTypeSize);
    storage.writeByteArray(new Array<number>(type.valueTypeSize).fill(0));
    const value=new Il2Cpp.ValueType(storage,type.type);
    value.field('hasValue').value=false;
    return call(value);
  }
  scan() {
    if(this.quests.method<boolean>('get_ExclusiveQuestCompletionInProgress',0).invoke())return null;
    const list=obj(this.quests.method<Obj>('GetQuestDataOfType',1).invoke(2));
    const listHeld=this.runtime.retain(list,'research:list');
    try {return this.nullable(this.nullableNow,now=>this.runtime.enumerate(list,16).map(value=>{
      obj(value,'Holoholo.Rpc.QuestProto');
      const held=this.runtime.retain(value,'research:scan');
      try {
        const context=this.runtime.number(value.field('QuestContext').value);
        const redeemable=context===2 && this.canRedeem.invoke(value,now)===true;
        const rewards=this.runtime.enumerate(obj(value.field('QuestRewards').value),32);
        const goal=value.field<Obj>('Goal').value;
        const conditions=goal.isNull()?[]:this.runtime.enumerate(obj(goal.field('Condition').value),32)
          .map(condition=>this.runtime.number(obj(condition,'Holoholo.Rpc.QuestConditionProto').field('Type').value));
        return {id:text(value.field('QuestId').value),context,
          type:this.runtime.number(value.field('QuestType').value),status:this.runtime.number(value.field('Status').value),redeemable,
          templateId:text(value.field('TemplateId').value),conditions,
          receivable:!redeemable || rewards.every(reward=>this.quests.method<boolean>('CanReceiveReward',1).invoke(reward)),
          encounterReward:rewards.some(reward=>this.runtime.number(reward.field('Type').value)===7)};
      }finally {held.free();}
    }));}finally {listHeld.free();}
  }
  claim(id:string):Obj {
    return this.nullable(this.nullableChoice,choice=>obj(this.complete.invoke(Il2Cpp.string(id),Il2Cpp.string(''),choice,new UInt64('0'))));
  }
  discard(id:string):Obj {return obj(this.remove.invoke(Il2Cpp.string(id)));}
  reward():Obj|null {
    const value=this.dangling.invoke(6,Il2Cpp.string(''));
    if(value instanceof Il2Cpp.Object&&value.isNull())return null;
    const reward=obj(value,'Holoholo.Rpc.QuestPokemonEncounterProto');
    if(this.runtime.number(reward.field('EncounterType').value)!==6)throw new ResearchProfileError('research_encounter_scope');
    if(this.runtime.number(reward.field('PokeBallOverride').value)!==0)return null;
    return reward;
  }
  encounterId(reward:Obj):string {
    const pokemon=obj(reward.field('Pokemon').value,'Holoholo.Rpc.PokemonProto');
    return method(pokemon,'get_Id',[],'System.UInt64').invoke()!.toString();
  }
  questId(reward:Obj):string {return text(reward.field('QuestId').value);}
  prepareCapture() {
    if(this.factory)return;
    const pokemon=this.runtime.klass('holo-game','Niantic.Holoholo.Quests.QuestMapPokemon');
    method(pokemon,'get_Destroyed',[],'System.Boolean');
    method(pokemon,'Destroy',[],'System.Void');
    const type=pokemon.tryNested('Factory');
    if(!type)throw new ResearchProfileError('research_factory_class_missing');
    const factory=this.runtime.factory(type);
    method(factory,'Create',['Holoholo.Rpc.QuestPokemonEncounterProto','Holoholo.Rpc.QuestEncounterOutProto'],
      'Niantic.Holoholo.Quests.QuestMapPokemon');
    this.factory=factory;
  }
  beginEncounter(reward:Obj):Obj {return obj(this.begin.invoke(reward));}
  createTarget(reward:Obj,result:Obj):Obj {
    if(!this.factory)throw new ResearchProfileError('research_factory_missing');
    const value=obj(this.factory.method<Obj>('Create',2).invoke(reward,result),'Niantic.Holoholo.Quests.QuestMapPokemon');
    const id=value.method<UInt64>('get_EncounterId',0).invoke().toString();
    if(id!==this.encounterId(reward)) {
      if(!value.method<boolean>('get_Destroyed',0).invoke())value.method('Destroy',0).invoke();
      throw new ResearchProfileError('research_target_id_mismatch');
    }
    return value;
  }
}
