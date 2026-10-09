const DEFAULT_CATCHES=4800,DEFAULT_SPINS=2400,MAXIMUM=2147483647;

function limit(value,fallback) {
  if(value===undefined)return fallback;
  if(!Number.isInteger(value)||value<1||value>MAXIMUM)throw Error('Invalid session limit');
  return value;
}
function read(parameters) {
  return {maxCatches:limit(parameters.maxCatches,DEFAULT_CATCHES),maxSpins:limit(parameters.maxSpins,DEFAULT_SPINS)};
}
function state(config,counts) {
  const maxCatches=config.maxCatches??DEFAULT_CATCHES,maxSpins=config.maxSpins??DEFAULT_SPINS;
  return {maxCatches,maxSpins,catchLimitReached:config.autoCatch===true&&counts.caught>=maxCatches,
    spinLimitReached:config.autoSpin===true&&counts.spun>=maxSpins};
}
module.exports={DEFAULT_CATCHES,DEFAULT_SPINS,MAXIMUM,read,state};
