// Session-bound transfer policy: ambiguous data never authorizes deletion.
const {ownedId, classifyPokemon} = require('./pokemon-inspection.cjs');
function transferVerdict(data, options, source) {
  if (data.egg) return {decision:'retain',reasons:['egg'],transferAllowed:false};
  const inspected = classifyPokemon(data);
  if (inspected.decision === 'unknown' || !Number.isInteger(data.dexId) || data.dexId <= 0
      || typeof data.buddy !== 'boolean' || typeof data.fusion !== 'boolean'
      || typeof data.limited !== 'boolean' || !Number.isInteger(data.pokemonClass)
      || data.pokemonClass < 0 || data.pokemonClass > 3
      || typeof options.keepShiny !== 'boolean' || typeof options.keepHundo !== 'boolean')
    return {decision:'unknown',reasons:['transfer_protection_data_invalid'],transferAllowed:false};
  const reasons = [];
  if (options.keepFieldResearch !== undefined && typeof options.keepFieldResearch !== 'boolean'
      || options.keepFieldResearch === true && !['WILD','FIELD_RESEARCH','SAMPLE'].includes(source))
    return {decision:'unknown',reasons:['transfer_origin_invalid'],transferAllowed:false};
  if (options.keepFieldResearch && source === 'FIELD_RESEARCH') reasons.push('field_research');
  for (const [option, field] of [['keepDynamax','dynamax'],['keepHatched','hatched'],
    ['keepBackground','background'],['keepShinyBackground','background']]) {
    if (options[option] !== undefined && typeof options[option] !== 'boolean'
        || options[option] === true && typeof data[field] !== 'boolean')
      return {decision:'unknown',reasons:['transfer_protection_data_invalid'],transferAllowed:false};
  }
  if (data.shiny && options.keepShiny) reasons.push('shiny');
  if (inspected.hundo && options.keepHundo) reasons.push('hundo');
  if (options.keepDynamax && data.dynamax) reasons.push('dynamax');
  if (options.keepHatched && data.hatched) reasons.push('hatched');
  if (options.keepBackground && data.background) reasons.push('background');
  if (options.keepShinyBackground && data.shiny && data.background) reasons.push('shiny_background');
  for (const key of ['favorite','egg','deployed','buddy','fusion','limited']) if (data[key]) reasons.push(key);
  if (data.pokemonClass !== 0) reasons.push('special_class');
  return {decision:reasons.length ? 'retain' : 'candidate',reasons:reasons.length ? reasons : ['session_candidate'],
    transferAllowed:reasons.length === 0};
}
class SessionTransferLedger {
  constructor(sessionId, limit = 3, capacity = 50000) {
    if (typeof sessionId !== 'string' || !/^[A-Za-z0-9_-]{8,96}$/.test(sessionId)) throw Error('Invalid session ID');
    if (!Number.isInteger(limit) || limit < 0 || limit > 1000) throw Error('Invalid transfer limit');
    if (!Number.isInteger(capacity) || capacity < 1) throw Error('Invalid session capacity');
    this.sessionId = sessionId; this.limit = limit; this.capacity = capacity;
    this.seenOwned = new Set(); this.captures = new Map(); this.requests = new Map();
    this.sequence = 0; this.closed = false; this.confirmed = 0;
  }
  issueCapture(encounterId, sentAt, source = 'WILD') {
    if (this.closed || this.requests.size || !ownedId(encounterId) || !Number.isFinite(sentAt)
        || !['WILD','FIELD_RESEARCH'].includes(source))
      throw Error('Invalid or overlapping capture request');
    const request = Object.freeze({sessionId:this.sessionId, requestId:this.sessionId+':'+(++this.sequence), encounterId, sentAt, source});
    this.requests.set(request.requestId, request); return request;
  }
  abandonCapture(request) {
    const issued = this.requests.get(request?.requestId);
    if (this.closed || !issued || request.sessionId !== this.sessionId
        || request.encounterId !== issued.encounterId || request.sentAt !== issued.sentAt)
      throw Error('Invalid abandoned capture request');
    this.requests.delete(issued.requestId);
  }
  settleCapture(response) {
    const reject = reason => ({accepted:false,reason});
    if (this.closed || response?.sessionId !== this.sessionId) return reject('foreign_or_closed_session');
    const request = this.requests.get(response.requestId);
    if (!request) return reject('unissued_or_consumed_request');
    // Every response consumes its request, including escapes and malformed IDs.
    this.requests.delete(request.requestId);
    if (response.encounterId !== request.encounterId || response.sentAt !== request.sentAt)
      return reject('request_mismatch');
    if (!Number.isFinite(response.completedAt) || response.completedAt < request.sentAt
        || response.completedAt-request.sentAt > 18000) return reject('expired_or_invalid_response_time');
    if (response.responseClass !== 'Holoholo.Rpc.CatchPokemonOutProto') return reject('wrong_response_class');
    if (response.freshPromise !== true) return reject('already_completed_or_failed_promise');
    if (response.status !== 1) return reject('capture_not_successful');
    const id = response.ownedId;
    if (!ownedId(id)) return reject('invalid_captured_id');
    if (this.seenOwned.has(id)) return reject('duplicate_capture_owned_id');
    if (this.seenOwned.size >= this.capacity) throw Error('Session capacity exceeded');
    // Deduplication survives retaining/releasing, but nothing is persisted.
    this.seenOwned.add(id); this.captures.set(id, {...request,state:'captured'});
    return {accepted:true,reason:'current_session_capture_response'};
  }
  authorize(id, encounterId) {
    const record = this.captures.get(id);
    return !this.closed && record?.sessionId === this.sessionId && record?.state === 'captured'
      && record.encounterId === encounterId && (this.limit === 0 || this.confirmed < this.limit);
  }
  captureSource(id, encounterId) {
    return this.authorize(id, encounterId) ? this.captures.get(id).source : null;
  }
  begin(id, encounterId) {
    if (!this.authorize(id, encounterId)) throw Error('Transfer outside current-session scope or limit');
    this.captures.get(id).state = 'sent';
  }
  finish(id, confirmed) {
    const record = this.captures.get(id);
    if (!record || this.closed || (confirmed && record.state !== 'sent')) throw Error('Invalid transfer confirmation');
    if (confirmed) this.confirmed++;
    this.captures.delete(id);
  }
  close() { this.closed = true; this.requests.clear(); this.seenOwned.clear(); this.captures.clear(); }
}
module.exports = {transferVerdict, SessionTransferLedger};
