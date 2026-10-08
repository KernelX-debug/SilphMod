// Own accounting policy. Rewards come from confirmed game responses, never
// from fixed rates, account-balance differences or locally applied multipliers.
const MAX_COMPONENTS = 64;
function amount(value) {
  if (!Number.isSafeInteger(value) || value < 0) throw Error('invalid_reward_amount');
  return value;
}
function sum(values) {
  if (!Array.isArray(values) || values.length > MAX_COMPONENTS) throw Error('invalid_reward_list');
  return values.reduce((total, value) => amount(total + amount(value)), 0);
}
function capture(score) {
  if (!score || typeof score !== 'object') throw Error('missing_capture_score');
  return {xp: amount(sum(score.exp) + amount(score.mega) + amount(score.tempEvo)),
    stardust: sum(score.stardust)};
}
function spin(xpAwarded, loot, bonusLoot) {
  let xp = amount(xpAwarded), stardust = 0;
  for (const items of [loot, bonusLoot]) {
    if (!Array.isArray(items) || items.length > MAX_COMPONENTS) throw Error('invalid_loot_list');
    for (const item of items) {
      // A protobuf oneof must not claim two different reward kinds.
      if (!item || typeof item.experience !== 'boolean' || typeof item.stardust !== 'boolean'
          || item.experience && item.stardust) throw Error('invalid_loot_kind');
      if (item.experience) xp = amount(xp + amount(item.count));
      else if (item.stardust) stardust = amount(stardust + amount(item.count));
    }
  }
  return {xp, stardust};
}
class SessionRewards {
  constructor() { this.xp = 0; this.stardust = 0; this.lastSequence = 0; this.missing = 0; }
  // One pending request at a time. A watermark avoids an unbounded ID set in
  // long sessions; old/duplicate/out-of-order responses can never add twice.
  settle(sequence, reward) {
    if (!Number.isSafeInteger(sequence) || sequence <= 0) throw Error('invalid_reward_sequence');
    if (sequence <= this.lastSequence) return {added: false, reason: 'duplicate_or_old'};
    if (reward === null) {
      this.missing = amount(this.missing + 1); this.lastSequence = sequence;
      return {added: false, reason: 'unreadable_reward'};
    }
    const xp = amount(this.xp + amount(reward.xp));
    const stardust = amount(this.stardust + amount(reward.stardust));
    this.xp = xp; this.stardust = stardust; this.lastSequence = sequence;
    return {added: true, reason: 'confirmed_response'};
  }
  snapshot() { return {xp: this.xp, stardust: this.stardust, missingRewardReceipts: this.missing}; }
}
module.exports = {MAX_COMPONENTS, amount, capture, spin, SessionRewards};
