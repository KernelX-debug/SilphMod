const ITEMS = Object.freeze({RazzBerry:701, NanabBerry:703, PinapBerry:705, GoldenRazzBerry:706, GoldenPinapBerry:708});
const DEFAULT_ORDER = Object.freeze(['RazzBerry', 'PinapBerry', 'NanabBerry']);
function order(value = DEFAULT_ORDER) {
  if (!Array.isArray(value) || value.length > 5 || new Set(value).size !== value.length
      || value.some(name => !Object.prototype.hasOwnProperty.call(ITEMS, name))) throw Error('Invalid berry priority');
  return [...value];
}
function activeItem(value) {
  if (!Number.isInteger(value) || (value !== 0 && !Object.values(ITEMS).includes(value)))
    throw Error('Invalid active encounter item');
  return value;
}
class BerryCycle {
  constructor(initialItem = 0) {
    this.active = activeItem(initialItem); this.attempted = false; this.suppressed = false;
  }
  choose(priority, counts) {
    if (this.active !== 0 || this.attempted || this.suppressed) return null;
    return priority.find(name => Number.isInteger(counts[name]) && counts[name] > 0) ?? null;
  }
  request() {
    if (this.active !== 0 || this.attempted || this.suppressed) throw Error('Duplicate berry request');
    this.attempted = true;
  }
  settle(status, item, requested) {
    if (!Number.isInteger(status) || status < 0 || status > 4) {
      this.suppressed = true; return {action:'continue', reason:'unknown_status', consumed:false};
    }
    if (status === 1) return {action:'skip', reason:'encounter_completed', consumed:false};
    if (status === 3) return {action:'continue', reason:'no_item_in_inventory', consumed:false};
    if (status === 4) { this.suppressed = true; return {action:'continue', reason:'invalid_item_category', consumed:false}; }
    try { this.active = activeItem(item); }
    catch { this.suppressed = true; return {action:'continue', reason:'invalid_active_item', consumed:false}; }
    if (this.active === 0 || (status === 0 && this.active !== requested)) {
      this.suppressed = true; return {action:'continue', reason:'unconfirmed_active_item', consumed:false};
    }
    return {action:'continue', reason:status === 0 ? 'applied' : 'active_item_exists', consumed:status === 0};
  }
  reject() { this.suppressed = true; }
  capture(status) {
    if (status === 2) { this.active = 0; this.attempted = false; }
  }
}
module.exports = {ITEMS, DEFAULT_ORDER, order, activeItem, BerryCycle};
