'use strict';

const REPORT_MS=30000;
function scanReason(scan) {
  if(scan.blocked)return scan.blocked;
  if(scan.selectedId)return 'selected';
  if(scan.listed===0)return 'no_loaded_stops';
  if(scan.cooldown===scan.visited)return 'cooldown';
  if(scan.scanLimitReached)return 'scan_limit_reached';
  if(scan.cooldown>0&&scan.outOfRange>0)return 'cooldown_and_distance';
  if(scan.outOfRange>0)return 'out_of_range';
  return 'no_eligible_stop';
}
class SpinObserver {
  constructor() {this.lastScan=null;this.lastResponse=null;this.lastSuccess=null;this.nextReport=0;this.lastReason='';}
  response(now,id,status) {
    this.lastResponse={at:now,id,status};
    if(status===1)this.lastSuccess=now;
  }
  observe(now,scan,report) {
    const reason=scanReason(scan);
    this.lastScan={at:now,reason,rangeMeters:80,...scan};
    if(now>=this.nextReport||reason!==this.lastReason) {
      this.lastReason=reason;this.nextReport=now+REPORT_MS;
      report('spin_scan',this.snapshot(now));
    }
  }
  snapshot(now) {
    return {scan:this.lastScan,lastResponse:this.lastResponse,
      sinceLastSuccessMs:this.lastSuccess===null?null:Math.max(0,now-this.lastSuccess)};
  }
}
module.exports={REPORT_MS,scanReason,SpinObserver};
