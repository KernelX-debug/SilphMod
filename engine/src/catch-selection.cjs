const MAX_DEX = 1025;
function read(value) {
  if (value === undefined || value === null) return [];
  if (!Array.isArray(value) || value.length > MAX_DEX || value.some(id => !Number.isInteger(id) || id < 1 || id > MAX_DEX))
    throw Error('Invalid Only catch selection');
  return [...new Set(value)].sort((a, b) => a - b);
}
class SpeciesFilter {
  constructor(value) { this.ids = new Set(read(value)); this.active = this.ids.size > 0; }
  allows(id) { return !this.active || Number.isInteger(id) && this.ids.has(id); }
  allowsPokemon(readId, source = 'WILD') { return source === 'FIELD_RESEARCH' || !this.active || this.allows(readId()); }
}
module.exports = {MAX_DEX, read, SpeciesFilter};
