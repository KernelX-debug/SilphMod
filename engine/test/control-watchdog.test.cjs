'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const watchdog=require('../src/control-watchdog.cjs'),policy=require('../src/policy.cjs');
test('new recovery does not extend the 2.2-second permission to send game actions',()=>{
  assert.equal(watchdog.ACTION_LEASE_MS,2200);
  assert.equal(policy.leaseValid(12200,10000,true,true,false),true);
  assert.equal(policy.leaseValid(12201,10000,true,true,false),false);
  assert.deepEqual(watchdog.state(14000,10000,true),{phase:'delayed',ageMs:4000});
});
test('a four-second controller delay pauses rather than tearing down a healthy game',()=>{
  assert.equal(watchdog.state(14000,10000,true).phase,'delayed');
  assert.equal(watchdog.state(14000,10000,false).phase,'expired');
});
test('genuine controller loss still expires at a bounded ten seconds',()=>{
  assert.equal(watchdog.state(20000,10000,true).phase,'delayed');
  assert.equal(watchdog.state(20001,10000,true).phase,'expired');
});
test('negative time deltas do not authorize activity and corrupt clocks fail closed',()=>{
  assert.equal(watchdog.state(9999,10000,true).phase,'delayed');
  assert.equal(policy.leaseValid(9999,10000,true,true,false),false);
  for(const now of [NaN,Infinity,-Infinity])assert.equal(watchdog.state(now,10000,true).phase,'expired');
});
