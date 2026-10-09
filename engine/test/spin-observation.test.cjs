const test=require('node:test'),assert=require('node:assert/strict');
const {scanReason,SpinObserver}=require('../src/spin-observation.cjs');
const empty={listed:0,visited:0,cooldown:0,outOfRange:0,scanLimitReached:false,selectedId:''};

test('scan reasons distinguish capacity, loaded stops, cooldowns and distance',()=>{
  for(const [scan,reason] of [
    [{blocked:'item_bag_full'},'item_bag_full'],[{blocked:'stop_cache_capacity'},'stop_cache_capacity'],
    [empty,'no_loaded_stops'],[{...empty,listed:10,visited:10,cooldown:10},'cooldown'],
    [{...empty,listed:10,visited:10,outOfRange:10},'out_of_range'],
    [{...empty,listed:10,visited:10,outOfRange:4,cooldown:6},'cooldown_and_distance'],
    [{...empty,listed:256,visited:256,outOfRange:256,scanLimitReached:true},'scan_limit_reached'],
    [{...empty,selectedId:'stop-1'},'selected'],
  ])assert.equal(scanReason(scan),reason);
});

test('unchanged scans update the health snapshot while their log emissions stay bounded',()=>{
  const observer=new SpinObserver(),events=[];
  for(let i=0;i<100;i++)observer.observe(10000+i*100,empty,(event,detail)=>events.push({event,...detail}));
  assert.equal(events.length,1);assert.equal(observer.snapshot(20000).scan.at,19900);
  observer.observe(40000,empty,(event,detail)=>events.push({event,...detail}));assert.equal(events.length,2);
});

test('reason transitions are logged and successful responses alone reset the spin-gap timer',()=>{
  const observer=new SpinObserver(),events=[];
  observer.observe(10000,empty,(event,detail)=>events.push({event,...detail}));
  observer.observe(11000,{blocked:'item_bag_full'},(event,detail)=>events.push({event,...detail}));
  assert.equal(events.length,2);assert.equal(observer.snapshot(12000).sinceLastSuccessMs,null);
  observer.response(13000,'stop-1',1);observer.response(14000,'stop-2',2);
  const value=observer.snapshot(18000);assert.equal(value.sinceLastSuccessMs,5000);
  assert.deepEqual(value.lastResponse,{at:14000,id:'stop-2',status:2});
});

test('observation leaves its input and the existing scheduling decisions untouched',()=>{
  const scan=Object.freeze({...empty,listed:2,visited:2,cooldown:1,outOfRange:1,earliestCooldownMs:7000,closestCheckedMeters:95});
  const observer=new SpinObserver();observer.observe(10000,scan,()=>{});
  assert.equal(scan.rangeMeters,undefined);assert.equal(observer.snapshot(10000).scan.rangeMeters,80);
  assert.equal(observer.snapshot(10000).scan.earliestCooldownMs,7000);
});
