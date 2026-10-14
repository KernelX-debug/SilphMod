const FIELD_CONTEXT = 2, FIELD_ENCOUNTER = 6;
const UNSUPPORTED = new Set([7, 8, 11, 14, 15, 18, 20, 21, 22, 23, 24, 25,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 38, 39, 40, 43, 50, 51, 53, 54,
  55, 56, 57, 58, 59, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 74]);
const UNSUPPORTED_CONDITIONS = new Set([6, 7, 9, 10, 18, 19, 22, 23, 27, 38, 41, 44, 48, 49, 55, 56, 58, 60, 61, 62]);
const SUPPORTED = new Set([1, 2, 3, 4, 5, 6, 9, 10, 12, 13, 16, 17, 19, 37, 41, 42, 44, 45, 60, 66]);
const SUPPORTED_CONDITIONS = new Set([1, 2, 3, 4, 5, 8, 11, 12, 13, 14, 15, 16, 17, 20, 21,
  24, 25, 26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 42, 43, 45, 46, 47,
  50, 51, 52, 53, 54, 57, 59, 63, 64, 65]);
function decide(task, config) {
  if (!task || task.context !== FIELD_CONTEXT || typeof task.id !== 'string' || !task.id.trim())
    return {action: 'keep', reason: 'outside_field_research'};
  if (task.redeemable === true) return {action: 'claim', reason: 'game_can_redeem'};
  if (!config.discardUnsupportedResearch || task.status !== 1 || task.redeemable !== false)
    return {action: 'keep', reason: 'not_removable'};
  if (task.type === 46) return {action: 'keep', reason: 'ar_slot_preserved'};
  if (UNSUPPORTED.has(task.type)) return {action: 'remove', reason: 'unsupported_activity'};
  if (!SUPPORTED.has(task.type)) return {action: 'remove', reason: 'unknown_activity'};
  if (Array.isArray(task.conditions) && task.conditions.some(type => UNSUPPORTED_CONDITIONS.has(type)))
    return {action: 'remove', reason: 'unsupported_condition'};
  if (Array.isArray(task.conditions) && task.conditions.some(type => !SUPPORTED_CONDITIONS.has(type)))
    return {action: 'remove', reason: 'unknown_condition'};
  if ((task.type === 10 && !config.transferEnabled) || (task.type === 13 && !config.useBerries))
    return {action: 'remove', reason: 'required_feature_disabled'};
  return {action: 'keep', reason: 'supported_activity'};
}
class ResearchQueue {
  constructor() { this.nextScan = 0; this.nextAction = 0; this.entries = new Map(); this.quarantined = new Set(); this.disabled = false; }
  due(now) { return !this.disabled && now >= this.nextScan && now >= this.nextAction; }
  scanned(now, ids) {
    this.nextScan = now + 10000;
    const present = new Set(ids);
    for (const [key, entry] of this.entries)
      if (!present.has(entry.id) && now >= entry.until) this.entries.delete(key);
  }
  refresh(now) { this.nextScan = Math.min(this.nextScan, now + 500); }
  available(kind, id, now) {
    const entry = this.entries.get(kind + ':' + id);
    return !this.disabled && !this.quarantined.has(id) && this.entries.size < 256 && (!entry || !entry.terminal && now >= entry.until);
  }
  sent(kind, id, now) {
    this.entries.set(kind + ':' + id, {id, until: now + 60000, terminal: false});
    this.nextAction = now + 3000;
  }
  settled(kind, id, now, status) {
    this.entries.set(kind + ':' + id, {id, until: now + 60000,
      terminal: status === 1 || kind === 'claim' && [2, 4].includes(status) || kind === 'remove' && status === 2});
    this.refresh(now);
  }
  uncertain(kind, id, now) {
    if (typeof kind !== 'string' || typeof id !== 'string' || !id.trim() || !Number.isFinite(now)
        || this.quarantined.size >= 256) { this.disabled = true; return; }
    this.quarantined.add(id);
    this.nextAction = now + 3000;
    this.nextScan = Math.max(this.nextScan, this.nextAction);
  }
}
module.exports = {FIELD_CONTEXT, FIELD_ENCOUNTER, decide, ResearchQueue};
