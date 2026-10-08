// Own session/liveness policies, independent of any commercial engine.
function playerIdValid(value) {
  return typeof value === 'string' && /^[^\x00-\x20\x7f]{1,256}$/.test(value);
}
function pruneExpired(cache, now) {
  for (const [key, until] of cache) if (until <= now) cache.delete(key);
}
module.exports = {playerIdValid, pruneExpired};
