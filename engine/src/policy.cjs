// Own policy code, shared with executable mock tests. No game or mod dependency.
const {ITEMS} = require('./item-catalog.cjs');
const CLEANUP_TRIGGER = 'EXCESS_FROM_START';
function discardPlan(limits, counts, maxBatch=50) {
  if (!Number.isInteger(maxBatch) || maxBatch < 1 || maxBatch > 50) return null;
  for (const [name, keep] of Object.entries(limits || {})) {
    if (!Object.prototype.hasOwnProperty.call(ITEMS, name) || !Number.isInteger(keep) || keep < 0 || keep > 10000) continue;
    const count = counts[name];
    if (!Number.isInteger(count) || count <= keep) continue;
    return {name, item:ITEMS[name], count:Math.min(count-keep, maxBatch), before:count, keep};
  }
  return null;
}
function catchDecision(status, attempt, maxAttempts) {
  if (status === 1) return 'caught';
  if (status === 3) return 'fled';
  if ([2,4].includes(status) && (maxAttempts === 0 || attempt < maxAttempts)) return 'retry';
  if ([2,4].includes(status)) return 'exhausted';
  if (status === 0) return 'rejected';
  if (status === 5) return 'replacement';
  return 'error';
}
function leaseValid(now, lastBeat, enabled, foreground, stopped) {
  return !stopped && enabled && foreground && now >= lastBeat && now-lastBeat <= 2200;
}
function discardConfirmed(status, before, count, remaining, keep) {
  return status === 1 && [before,count,remaining,keep].every(Number.isInteger)
    && keep >= 0 && count > 0 && count <= 50 && before-count >= keep
    && remaining >= keep && remaining <= before-count;
}
module.exports = {ITEMS, CLEANUP_TRIGGER, discardPlan, catchDecision, leaseValid, discardConfirmed};
