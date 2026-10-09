const PROFILES = Object.freeze({
  NORMAL: Object.freeze({grade: 1, reticleSize: 1.1, hitBullseye: true}),
  NICE: Object.freeze({grade: 2, reticleSize: 0.85, hitBullseye: true}),
  GREAT: Object.freeze({grade: 3, reticleSize: 0.5, hitBullseye: true}),
  EXCELLENT: Object.freeze({grade: 4, reticleSize: 0.05, hitBullseye: true})
});
function profile(quality = 'GREAT') {
  if (typeof quality !== 'string' || !Object.prototype.hasOwnProperty.call(PROFILES, quality))
    throw Error('Invalid throw quality');
  return PROFILES[quality];
}
module.exports = {PROFILES, profile};
