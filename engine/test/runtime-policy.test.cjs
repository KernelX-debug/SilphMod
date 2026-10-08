const test=require('node:test'),assert=require('node:assert/strict');
const {playerIdValid,pruneExpired}=require('../src/runtime-policy.cjs');
test('player identity rejects missing, whitespace, control and oversized tokens',()=>{
  for(const value of [null,undefined,0,'',' a','a b','a\n','a\x7f','a'.repeat(257)])assert.equal(playerIdValid(value),false);
  for(const value of ['player-A','account.id:42','a'.repeat(256)])assert.equal(playerIdValid(value),true);
});
test('cache maintenance removes only expired entries and never drops live cooldowns',()=>{
  const cache=new Map([['expired',9],['boundary',10],['live',11]]);pruneExpired(cache,10);
  assert.deepEqual([...cache],[['live',11]]);
});
