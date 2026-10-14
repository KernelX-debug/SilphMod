'use strict';

const FALLBACK_METERS=40, MAX_VALIDATED_METERS=50, MAX_SAMPLE_AGE_MS=65000, MAX_TRANSFER_DEFERRAL_MS=3000;

function range(sample,now) {
  const fallback=reason=>({meters:FALLBACK_METERS,source:'fallback',reason,
    maximumValidatedMeters:MAX_VALIDATED_METERS,dynamicLimitApplied:false,bonusApplied:false});
  if(!sample||sample.base?.available!==true)return fallback('settings_unavailable');
  const age=now-sample.sampledAt;
  if(!Number.isFinite(now)||!Number.isFinite(sample.sampledAt)||age<0||age>MAX_SAMPLE_AGE_MS)
    return fallback('settings_stale');
  const meters=sample.base.values?.EncounterRangeMeters;
  if(typeof meters!=='number'||!Number.isFinite(meters)||meters<=0)return fallback('invalid_encounter_setting');
  if(meters>MAX_VALIDATED_METERS)return fallback('outside_validated_range');
  return {meters,source:'IGlobalSettings.Settings.MapSettings.EncounterRangeMeters',reason:'fresh_validated_setting',
    maximumValidatedMeters:MAX_VALIDATED_METERS,dynamicLimitApplied:true,bonusApplied:false};
}

function captureFirst(hasTarget,oldestOwnedAt,now) {
  if(!hasTarget)return false;
  if(oldestOwnedAt===null)return true;
  const age=now-oldestOwnedAt;
  return Number.isFinite(age)&&age>=0&&age<MAX_TRANSFER_DEFERRAL_MS;
}

module.exports={range,captureFirst,FALLBACK_METERS,MAX_VALIDATED_METERS,MAX_SAMPLE_AGE_MS,MAX_TRANSFER_DEFERRAL_MS};
