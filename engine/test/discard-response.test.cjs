const test=require('node:test'),assert=require('node:assert/strict');
const {assess}=require('../src/discard-response.cjs');
test('confirmed discard remains bounded by the original floor and batch',()=>{
  assert.equal(assess(1,100,50,50,40).action,'confirmed');
  assert.equal(assess(1,1,1,0,0).action,'confirmed');
});
test('not enough copies/nonrecyclable/unset valid results pause recycling, never retry',()=>{
  for(const status of [0,2,3])assert.equal(assess(status,1,1,1,0).action,'pause');
});
test('concurrent replenishment can invalidate a count comparison without killing the session',()=>{
  assert.deepEqual(assess(1,1,1,1,0),{action:'pause',reason:'remaining_count_mismatch'});
});
test('acknowledged floor violation, corrupt data, unknown statuses and bad requests stay fatal',()=>{
  for(const args of [[1,100,50,39,40],[7,1,1,0,0],[1,1,1,-1,0],[1,1,1,NaN,0],
    [1,1,1,0,1],[2,1,51,1,0],[1,1,0,1,0]])assert.equal(assess(...args).action,'fatal');
});
