const {discardConfirmed} = require('./policy.cjs');
// Class/layout/pointer errors never reach this policy: those remain fatal.
// A valid but unconfirmed reply pauses ALL recycling for this session. No retry,
// guessed success, relaxed minimum or substitute item is authorized here.
function assess(status, before, count, remaining, keep) {
  if (![before,count,keep].every(Number.isInteger) || before < 0 || keep < 0
      || count <= 0 || count > 50 || before - count < keep)
    return {action: 'fatal', reason: 'invalid_discard_request'};
  if (!Number.isInteger(status) || ![0,1,2,3].includes(status)
      || !Number.isInteger(remaining) || remaining < 0)
    return {action: 'fatal', reason: 'invalid_discard_response'};
  if (status === 1 && remaining < keep)
    return {action: 'fatal', reason: 'confirmed_minimum_breached'};
  if (discardConfirmed(status, before, count, remaining, keep))
    return {action: 'confirmed', reason: 'confirmed'};
  return {action: 'pause', reason: status === 2 ? 'not_enough_copies'
    : status === 3 ? 'item_not_recyclable' : status === 0 ? 'unset_result' : 'remaining_count_mismatch'};
}
module.exports = {assess};
