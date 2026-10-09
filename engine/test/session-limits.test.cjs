const test=require('node:test'),assert=require('node:assert/strict');
const limits=require('../src/session-limits.cjs');

test('session limits default to 4800 confirmed catches and 2400 confirmed spins',()=>{
  assert.deepEqual(limits.read({}),{maxCatches:4800,maxSpins:2400});
});
test('both limits accept explicit positive integers without truncating large values',()=>{
  assert.deepEqual(limits.read({maxCatches:1,maxSpins:2147483647}),{maxCatches:1,maxSpins:2147483647});
});
test('invalid limits cannot become unlimited or wrap around',()=>{
  for(const value of [0,-1,1.1,2147483648,NaN,Infinity,'4800',null,true]) {
    assert.throws(()=>limits.read({maxCatches:value}));
    assert.throws(()=>limits.read({maxSpins:value}));
  }
});
test('capture and spin limits are independent and failures do not consume the other budget',()=>{
  const config={...limits.read({maxCatches:2,maxSpins:3}),autoCatch:true,autoSpin:true};
  assert.deepEqual(limits.state(config,{caught:1,spun:2,fled:100}),{
    maxCatches:2,maxSpins:3,catchLimitReached:false,spinLimitReached:false});
  assert.equal(limits.state(config,{caught:2,spun:2}).catchLimitReached,true);
  assert.equal(limits.state(config,{caught:2,spun:2}).spinLimitReached,false);
  assert.equal(limits.state(config,{caught:0,spun:3}).spinLimitReached,true);
});
test('disabled features do not report a reached limit and new session counters start fresh',()=>{
  const config={maxCatches:1,maxSpins:1,autoCatch:false,autoSpin:false};
  assert.equal(limits.state(config,{caught:1,spun:1}).catchLimitReached,false);
  assert.equal(limits.state(config,{caught:1,spun:1}).spinLimitReached,false);
  assert.equal(limits.state({...config,autoCatch:true,autoSpin:true},{caught:0,spun:0}).catchLimitReached,false);
});
