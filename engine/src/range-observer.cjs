'use strict';

const INTERVAL_MS=30000, CANDIDATE_LIMIT=16;
class RangeProfileError extends Error {}
function type(runtime,assembly,name) {
  const value=runtime.tryClass(assembly,name);
  if(!value||value.fullName!==name)throw new RangeProfileError('class_unavailable:'+name);
  return value;
}
function field(runtime,owner,name,expected) {
  const value=owner.tryField(name),bytes=expected==='Holoholo.Rpc.PlayerBonusType'?4:8;
  if(!value||value.isStatic||value.type.name!==expected||!Number.isInteger(value.offset)||value.offset<runtime.headerSize
      ||Number.isInteger(owner.instanceSize)&&value.offset+bytes>owner.instanceSize)
    throw new RangeProfileError('field_mismatch:'+owner.fullName+'.'+name);
}
function call(owner,name,result) {
  const value=owner.tryMethod(name,0);
  if(!value||value.isStatic||value.virtualAddress.isNull()||value.parameters.length!==0||value.returnType.name!==result)
    throw new RangeProfileError('method_mismatch:'+name);
  return value.invoke();
}
function amount(value,name) {
  if(typeof value!=='number'||!Number.isFinite(value)||value<0||value>10000)
    throw new RangeProfileError('invalid_meter_value:'+name);
  return value;
}
function position(value) {
  if(!value||!Number.isFinite(value.latitude)||!Number.isFinite(value.longitude)||Math.abs(value.latitude)>90||Math.abs(value.longitude)>180)
    throw new RangeProfileError('invalid_location');
  return {latitude:value.latitude,longitude:value.longitude};
}
function meters(a,b) {
  return 111320*Math.hypot(b.latitude-a.latitude,(b.longitude-a.longitude)*Math.cos(a.latitude*Math.PI/180));
}
function distanceBuckets(rows,limit) {
  const result={total:rows.length,within40:0,from40To80:0,from80To140:0,beyond140:0,withinConfiguredEncounter:null};
  if(typeof limit==='number'&&Number.isFinite(limit)&&limit>0)result.withinConfiguredEncounter=0;
  for(const row of rows) {
    const d=row.distanceMeters;
    if(!Number.isFinite(d)||d<0)throw new RangeProfileError('invalid_candidate_distance');
    if(d<=40)result.within40++;else if(d<=80)result.from40To80++;else if(d<=140)result.from80To140++;else result.beyond140++;
    if(result.withinConfiguredEncounter!==null&&d<=limit)result.withinConfiguredEncounter++;
  }
  return result;
}
class RangeObserver {
  constructor(runtime,report) {
    this.runtime=runtime;this.report=report;this.nextAt=0;this.closed=false;this.last=null;this.references=0;this.peakReferences=0;this.samples=0;
  }
  due(now) {return !this.closed&&now>=this.nextAt;}
  part(work) {
    const held=[],runtime=this.runtime;
    const keep=(value,expected)=>{
      if(!runtime.valid(value)||expected&&value.class.fullName!==expected)throw new RangeProfileError('object_mismatch:'+(expected??'service'));
      const reference=runtime.retain(value,'range:read');held.push(reference);this.references++;
      this.peakReferences=Math.max(this.peakReferences,this.references);return value;
    };
    const service=name=>{
      const schema=type(runtime,'holo-game',name),value=runtime.service(schema);
      if(!runtime.valid(value))throw new RangeProfileError('service_unavailable:'+name);
      return keep(value);
    };
    try {return {available:true,...work({keep,service})};}
    catch(error) {if(!(error instanceof RangeProfileError))throw error;return {available:false,reason:error.message};}
    finally {
      let failure;
      for(const reference of held.reverse())try {reference.free();this.references--;}
      catch(error){failure??=error;}
      if(failure)throw failure;
    }
  }
  sample(now,avatar,localLimit) {
    if(!this.due(now))return null;
    this.nextAt=now+INTERVAL_MS;
    const runtime=this.runtime;
    const base=this.part(({keep,service})=>{
      const global=type(runtime,'holo-protos','Holoholo.Rpc.GlobalSettingsProto');
      const mapType=type(runtime,'holo-protos','Holoholo.Rpc.MapSettingsProto');
      field(runtime,global,'MapSettings','Holoholo.Rpc.MapSettingsProto');
      for(const name of ['PokemonVisibleRange','EncounterRangeMeters','PokemonDespawnRange'])field(runtime,mapType,name,'System.Double');
      const owner=service('Niantic.Holoholo.IGlobalSettings');
      const settings=keep(call(owner,'get_Settings','Holoholo.Rpc.GlobalSettingsProto'),'Holoholo.Rpc.GlobalSettingsProto');
      const map=keep(settings.field('MapSettings').value,'Holoholo.Rpc.MapSettingsProto');
      const values=Object.fromEntries(['PokemonVisibleRange','EncounterRangeMeters','PokemonDespawnRange']
        .map(name=>[name,amount(map.field(name).value,name)]));
      return {source:'IGlobalSettings.Settings.MapSettings',values,nonzeroEncounterSetting:values.EncounterRangeMeters>0};
    });
    const proximity=this.part(({service})=>{
      const owner=service('Niantic.Holoholo.Map.IPlayerProximityService');
      const value=call(owner,'get_LastValidLocation','Niantic.LatLng');
      return {source:'IPlayerProximityService.LastValidLocation',location:position(runtime.location(value)),freshnessKnown:false};
    });
    const adventure=this.part(({keep,service})=>{
      const owner=service('Niantic.Holoholo.AdventureEffect.IAdventureEffectService');
      const proto=call(owner,'get_ActiveAdventureEffectProto','Holoholo.Rpc.AppliedBonusProto');
      if(runtime.isNull(proto))return {effectPresent:false,spaceBonusPresent:false};
      const bonusType=type(runtime,'holo-protos','Holoholo.Rpc.AppliedBonusProto');
      field(runtime,bonusType,'Effect','Holoholo.Rpc.AppliedBonusEffectProto');
      field(runtime,bonusType,'BonusType','Holoholo.Rpc.PlayerBonusType');
      field(runtime,bonusType,'ExpirationTimeMs','System.Int64');
      const bonus=keep(proto,'Holoholo.Rpc.AppliedBonusProto');
      const bonusId=runtime.number(bonus.field('BonusType').value),expires=runtime.number(bonus.field('ExpirationTimeMs').value);
      if(!Number.isInteger(bonusId)||bonusId<0||!Number.isSafeInteger(expires)||expires<0)throw new RangeProfileError('invalid_bonus_identity');
      const metadata={effectPresent:true,bonusType:bonusId,expirationTimeMs:expires,expiredAtSample:expires>0&&expires<=now};
      const effect=bonus.field('Effect').value;
      if(runtime.isNull(effect))return {...metadata,spaceBonusPresent:false};
      const heldEffect=keep(effect,'Holoholo.Rpc.AppliedBonusEffectProto');
      const space=call(heldEffect,'get_SpaceBonus','Holoholo.Rpc.AppliedSpaceBonusProto');
      if(runtime.isNull(space))return {...metadata,spaceBonusPresent:false};
      const schema=type(runtime,'holo-protos','Holoholo.Rpc.AppliedSpaceBonusProto');
      for(const name of ['PokemonVisibleRangeMeters','EncounterRangeMeters','ServerAllowableEncounterRangeMeters'])field(runtime,schema,name,'System.Double');
      const heldSpace=keep(space,'Holoholo.Rpc.AppliedSpaceBonusProto');
      const values=Object.fromEntries(['PokemonVisibleRangeMeters','EncounterRangeMeters','ServerAllowableEncounterRangeMeters']
        .map(name=>[name,amount(heldSpace.field(name).value,name)]));
      return {...metadata,spaceBonusPresent:true,rawValues:values,compositionAssumed:false};
    });
    const avatarPosition=position(avatar);
    this.last={sampledAt:now,avatarLocation:avatarPosition,base,proximity,adventure,
      positionDifferenceMeters:proximity.available?meters(avatarPosition,proximity.location):null,
      localCaptureLimitMeters:localLimit,dynamicLimitApplied:false,absoluteServerLimitKnown:false,
      readOnly:true,extraGameRequestsSent:0,sampleIntervalMs:INTERVAL_MS,sample:++this.samples};
    this.report('range_observation',this.last);return this.snapshot();
  }
  snapshot() {return this.last?JSON.parse(JSON.stringify(this.last)):null;}
  close() {this.closed=true;this.last=null;return {passed:this.references===0,remainingReferences:this.references,peakReferences:this.peakReferences,samples:this.samples,hooksInstalled:0};}
}
module.exports={RangeObserver,RangeProfileError,INTERVAL_MS,CANDIDATE_LIMIT,distanceBuckets,meters};
