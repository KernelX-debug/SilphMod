function distance(a, b) {
  const rad = Math.PI / 180;
  const lat = (b.latitude - a.latitude) * rad;
  const lng = (b.longitude - a.longitude) * rad;
  const h = Math.sin(lat / 2) ** 2 + Math.cos(a.latitude * rad)
    * Math.cos(b.latitude * rad) * Math.sin(lng / 2) ** 2;
  return 6371000 * 2 * Math.atan2(Math.sqrt(Math.min(1, h)), Math.sqrt(Math.max(0, 1 - h)));
}
class MapObserver {
  constructor() { this.previous = null; this.fingerprint = ''; this.changedAt = null; }
  observe(now, location, entries) {
    const fingerprint = [...new Set(entries.map(entry => entry.id))].sort().join(',');
    if (this.changedAt === null || fingerprint !== this.fingerprint) {
      this.changedAt = now; this.fingerprint = fingerprint;
    }
    const elapsed = this.previous ? now - this.previous.at : 0;
    const moved = elapsed > 0 ? distance(this.previous.location, location) : null;
    const valid = entries.filter(entry => Number.isFinite(entry.distanceMeters) && entry.distanceMeters >= 0);
    const result = {
      playerLocation: {latitude: location.latitude, longitude: location.longitude},
      observationIntervalMs: elapsed > 0 ? elapsed : null,
      movedMeters: moved,
      observedSpeedKmh: moved === null ? null : moved * 3600 / elapsed,
      closestVisibleMeters: valid.length ? Math.min(...valid.map(entry => entry.distanceMeters)) : null,
      visibleWithin40: valid.filter(entry => entry.distanceMeters <= 40).length,
      visible40To80: valid.filter(entry => entry.distanceMeters > 40 && entry.distanceMeters <= 80).length,
      visibleBeyond80: valid.filter(entry => entry.distanceMeters > 80).length,
      mapIdsUnchangedMs: Math.max(0, now - this.changedAt),
      mapSampleCount: entries.length
    };
    this.previous = {at: now, location: {...location}};
    return result;
  }
}
module.exports = {MapObserver};
