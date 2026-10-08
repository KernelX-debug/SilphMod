const test = require('node:test');
const assert = require('node:assert/strict');
const {discardPlan,catchDecision,leaseValid,discardConfirmed,ITEMS} = require('../src/policy.cjs');
const {CATALOG}=require('../src/item-catalog.cjs');
test('discard only explicit allowlisted items, never protected items', () => {
  assert.equal(discardPlan({MasterBall:0, WildBall:0},{MasterBall:5,WildBall:10}),null);
  assert.equal(discardPlan({}, {Potion:100}),null);
});
test('three ordinary balls and all five common berries have exact discard IDs',()=>{
  assert.deepEqual(['PokeBall','GreatBall','UltraBall'].map(name=>ITEMS[name]),[1,2,3]);
  assert.deepEqual(['RazzBerry','NanabBerry','PinapBerry','GoldenRazzBerry','GoldenPinapBerry'].map(name=>ITEMS[name]),[701,703,705,706,708]);
});
test('every catalog type has unique ID/name, and protected entries are absent from allowlist',()=>{
  assert.equal(CATALOG.length,50);assert.equal(new Set(CATALOG.map(x=>x.id)).size,50);
  assert.equal(new Set(CATALOG.map(x=>x.name)).size,50);assert.equal(Object.keys(ITEMS).length,33);
  for(const entry of CATALOG)assert.equal(ITEMS[entry.name],entry.discardable?entry.id:undefined);
});
test('explicit special consumable/evolution selection works but omission preserves it',()=>{
  for(const entry of CATALOG.filter(x=>x.special)) {
    const counts={[entry.name]:25};assert.equal(discardPlan({},counts),null);
    assert.equal(discardPlan({[entry.name]:20},counts).count,5);
  }
});
test('quota limits one batch without changing the selected keep floor',()=>{
  assert.equal(discardPlan({Potion:0},{Potion:100},20).count,20);
  assert.equal(discardPlan({Potion:90},{Potion:100},20).count,10);
  assert.equal(discardPlan({Potion:0},{Potion:100},0),null);
});
test('discard acknowledgement must preserve the configured minimum',()=>{
  assert.ok(discardConfirmed(1,296,50,246,200));
  for(const values of [[1,296,50,199,200],[1,296,50,297,200],[0,296,50,246,200],
    [1,296,51,245,200],[1,100,50,50,80],[1,296,50,NaN,200]])assert.equal(discardConfirmed(...values),false);
});
test('keep minimum and cap one batch', () => {
  assert.deepEqual(discardPlan({Potion:30},{Potion:200}),{name:'Potion',item:101,count:50,before:200,keep:30});
  assert.equal(discardPlan({Potion:30},{Potion:30}),null);
  assert.equal(discardPlan({Potion:-1},{Potion:100}),null);
  assert.equal(discardPlan({Potion:NaN},{Potion:100}),null);
});
test('capture counter only on confirmed success, bounded escapes', () => {
  assert.equal(catchDecision(1,1,4),'caught');
  assert.equal(catchDecision(3,1,4),'fled');
  assert.equal(catchDecision(2,3,4),'retry');
  assert.equal(catchDecision(2,4,4),'exhausted');
  assert.equal(catchDecision(0,1,4),'rejected');
  assert.equal(catchDecision(4,1,4),'retry');
  assert.equal(catchDecision(4,4,4),'exhausted');
  assert.equal(catchDecision(5,1,4),'replacement');
  for (const result of [-1,6,undefined,NaN]) assert.equal(catchDecision(result,1,4),'error');
});
test('foreground, user stop and heartbeat are fail-closed', () => {
  assert.equal(leaseValid(1000,500,true,true,false),true);
  assert.equal(leaseValid(3000,500,true,true,false),false);
  assert.equal(leaseValid(1000,500,true,false,false),false);
  assert.equal(leaseValid(1000,500,true,true,true),false);
  assert.equal(leaseValid(1000,500,false,true,false),false);
});
