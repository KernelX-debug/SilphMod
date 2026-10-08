const test = require('node:test'), assert = require('node:assert/strict');
const p = require('../src/session-rewards.cjs');
test('capture sums server score components and mega/temp evolution bonus exactly once', () => {
  assert.deepEqual(p.capture({exp:[100,1000,20,50],stardust:[100,50],mega:100,tempEvo:50}),
    {xp:1320,stardust:150});
});
test('event/lucky egg/star piece amounts are not multiplied or clamped to a base rate', () => {
  assert.deepEqual(p.capture({exp:[6000],stardust:[750],mega:0,tempEvo:0}),{xp:6000,stardust:750});
});
test('spin includes XP awarded and loot/bonus loot reward kinds, ignoring physical items', () => {
  assert.deepEqual(p.spin(100,[{count:500,experience:true,stardust:false},
    {count:3,experience:false,stardust:false}], [{count:100,experience:false,stardust:true}]),
  {xp:600,stardust:100});
});
test('zero rewards are confirmed data, not an absent receipt', () => {
  const ledger = new p.SessionRewards(); ledger.settle(1,p.capture({exp:[],stardust:[],mega:0,tempEvo:0}));
  assert.deepEqual(ledger.snapshot(),{xp:0,stardust:0,missingRewardReceipts:0});
});
test('session watermark prevents duplicate, retry replay and out-of-order double accounting', () => {
  const ledger = new p.SessionRewards();
  ledger.settle(4,{xp:1000,stardust:150});
  assert.equal(ledger.settle(4,{xp:1000,stardust:150}).added,false);
  assert.equal(ledger.settle(2,{xp:1000,stardust:150}).added,false);
  ledger.settle(8,{xp:500,stardust:0});
  assert.deepEqual(ledger.snapshot(),{xp:1500,stardust:150,missingRewardReceipts:0});
});
test('new session starts at zero and missing reward does not erase confirmed totals', () => {
  const ledger = new p.SessionRewards();ledger.settle(1,{xp:100,stardust:100});ledger.settle(2,null);ledger.settle(2,null);
  assert.deepEqual(ledger.snapshot(),{xp:100,stardust:100,missingRewardReceipts:1});
  assert.deepEqual(new p.SessionRewards().snapshot(),{xp:0,stardust:0,missingRewardReceipts:0});
});
test('invalid, negative, noninteger, overflowing or too large lists fail before accounting', () => {
  for (const n of [-1,NaN,Infinity,0.5,Number.MAX_SAFE_INTEGER+1])assert.throws(()=>p.amount(n));
  assert.throws(()=>p.capture(null));
  assert.throws(()=>p.capture({exp:Array(65).fill(1),stardust:[],mega:0,tempEvo:0}));
  assert.throws(()=>p.spin(0,[{count:1,experience:true,stardust:true}],[]));
  const ledger=new p.SessionRewards();ledger.settle(1,{xp:Number.MAX_SAFE_INTEGER,stardust:0});
  assert.throws(()=>ledger.settle(2,{xp:1,stardust:0}));assert.equal(ledger.lastSequence,1);
});
test('large sessions retain precise totals beyond 32 bit, no ID collection growth', () => {
  const ledger=new p.SessionRewards();for(let i=1;i<=10000;i++)ledger.settle(i,{xp:500000,stardust:100});
  assert.equal(ledger.xp,5000000000);assert.equal(ledger.stardust,1000000);
  assert.equal(Object.keys(ledger).length,4);
});
