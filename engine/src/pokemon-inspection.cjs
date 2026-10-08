// Read-only classification. A candidate is NEVER an authorization to transfer.
function ownedId(value) {
  if (typeof value !== 'string' || !/^[1-9][0-9]{0,19}$/.test(value)) return null;
  return BigInt(value) <= 18446744073709551615n ? value : null;
}
function classifyPokemon(data) {
  const reasons = [];
  const valid = ownedId(data.id) && ['attack','defense','stamina'].every(key =>
    Number.isInteger(data[key]) && data[key] >= 0 && data[key] <= 15)
    && ['shiny','favorite','egg','deployed'].every(key => typeof data[key] === 'boolean');
  if (!valid) return {decision:'unknown',reasons:['missing_or_invalid_data'],transferAllowed:false};
  const hundo = data.attack === 15 && data.defense === 15 && data.stamina === 15;
  if (data.shiny) reasons.push('shiny');
  if (hundo) reasons.push('hundo');
  if (data.favorite) reasons.push('favorite');
  if (data.egg) reasons.push('egg');
  if (data.deployed) reasons.push('deployed');
  return {decision:reasons.length ? 'retain' : 'review', reasons:reasons.length ? reasons : ['non_shiny_non_hundo'],
    hundo,shundo:hundo && data.shiny,ivPercent:Math.floor((data.attack+data.defense+data.stamina)*100/45),
    transferAllowed:false,unvalidatedProtections:['buddy','special_class','species_exceptions']};
}
module.exports = {ownedId,classifyPokemon};
