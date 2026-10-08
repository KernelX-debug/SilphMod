'use strict';

// A fresh permission to SEND actions still expires after 2.2 seconds.
// A brief controller delay pauses actions; it is not an immediate connection teardown.
const ACTION_LEASE_MS=2200, SILPH_STOP_MS=10000, LEGACY_STOP_MS=3000;
function state(now,lastBeat,recovery=false) {
  if(!Number.isFinite(now)||!Number.isFinite(lastBeat))return {phase:'expired',ageMs:null};
  if(now<lastBeat)return {phase:'delayed',ageMs:null};
  const ageMs=now-lastBeat;
  return {phase:ageMs>(recovery?SILPH_STOP_MS:LEGACY_STOP_MS)?'expired':ageMs>ACTION_LEASE_MS?'delayed':'healthy',ageMs};
}
module.exports={ACTION_LEASE_MS,SILPH_STOP_MS,LEGACY_STOP_MS,state};
