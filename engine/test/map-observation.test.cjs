const test=require('node:test'),assert=require('node:assert/strict');
const {MapObserver}=require('../src/map-observation.cjs');
const origin={latitude:0,longitude:0};

test('map observations include out-of-range distances, not only selectable targets',()=>{
  const observer=new MapObserver();
  const result=observer.observe(10000,origin,[{id:'1',distanceMeters:40},{id:'2',distanceMeters:60},
    {id:'3',distanceMeters:81},{id:'4',distanceMeters:NaN}]);
  assert.equal(result.closestVisibleMeters,40);assert.equal(result.visibleWithin40,1);
  assert.equal(result.visible40To80,1);assert.equal(result.visibleBeyond80,1);
  assert.equal(result.observedSpeedKmh,null);assert.equal(result.mapSampleCount,4);
});

test('movement speed uses sampled positions and elapsed time, not joystick configuration',()=>{
  const observer=new MapObserver();observer.observe(10000,origin,[]);
  const result=observer.observe(15000,{latitude:0.000312267,longitude:0},[]);
  assert.ok(Math.abs(result.observedSpeedKmh-25)<0.01);
  assert.equal(result.observationIntervalMs,5000);assert.ok(result.movedMeters>34);
  const stopped=observer.observe(20000,{latitude:0.000312267,longitude:0},[]);
  assert.equal(stopped.observedSpeedKmh,0);assert.equal(stopped.closestVisibleMeters,null);
});

test('unchanged map IDs ignore enumeration order and reset when membership changes',()=>{
  const observer=new MapObserver();
  observer.observe(10000,origin,[{id:'2',distanceMeters:80},{id:'1',distanceMeters:10}]);
  const next=observer.observe(15000,origin,[{id:'1',distanceMeters:90},{id:'2',distanceMeters:5}]);
  assert.equal(next.mapIdsUnchangedMs,5000);
  assert.equal(observer.observe(20000,origin,[{id:'2',distanceMeters:5}]).mapIdsUnchangedMs,0);
});

test('observations do not mutate source positions or ID arrays and handle dateline crossing',()=>{
  const observer=new MapObserver(),location={latitude:0,longitude:179.9999};
  const entries=Object.freeze([Object.freeze({id:'2',distanceMeters:15})]);
  observer.observe(10000,location,entries);location.longitude=-179.9999;
  const result=observer.observe(20000,location,entries);
  assert.ok(result.movedMeters>22&&result.movedMeters<23);
  assert.deepEqual(entries,[{id:'2',distanceMeters:15}]);
  assert.equal(observer.observe(20000,location,entries).observedSpeedKmh,null);
});
