const test=require('node:test'), assert=require('node:assert/strict');
const {ITEMS,DEFAULT_ORDER,order,BerryCycle}=require('../src/berry-policy.cjs');

test('only five supported encounter berries can be selected, with premium types opt-in',()=>{
  assert.deepEqual(ITEMS,{RazzBerry:701,NanabBerry:703,PinapBerry:705,GoldenRazzBerry:706,GoldenPinapBerry:708});
  assert.deepEqual(order(),['RazzBerry','PinapBerry','NanabBerry']);
  assert.ok(!DEFAULT_ORDER.includes('GoldenRazzBerry')&&!DEFAULT_ORDER.includes('GoldenPinapBerry'));
});
test('priority preserves explicit order, including an empty disabled selection',()=>{
  assert.deepEqual(order(['GoldenPinapBerry','PinapBerry']),['GoldenPinapBerry','PinapBerry']);
  assert.deepEqual(order([]),[]);
});
test('duplicates, non-berries and ambiguous priority input are rejected',()=>{
  for(const value of [null,'RazzBerry',['RazzBerry','RazzBerry'],['Poffin'],['BlukBerry'],[701],['RazzBerry ']])
    assert.throws(()=>order(value),/Invalid berry priority/);
});
test('selection uses the first available selected berry rather than the greatest count',()=>{
  const cycle=new BerryCycle();
  assert.equal(cycle.choose(order(),{RazzBerry:1,PinapBerry:100,NanabBerry:50}),'RazzBerry');
  assert.equal(cycle.choose(order(),{RazzBerry:0,PinapBerry:5}),'PinapBerry');
  assert.equal(cycle.choose(order(),{GoldenRazzBerry:100}),null);
});
test('invalid or absent stock never authorizes a berry request',()=>{
  const cycle=new BerryCycle();
  for(const value of [0,-1,NaN,Infinity,undefined,'5',0.5])
    assert.equal(cycle.choose(['RazzBerry'],{RazzBerry:value}),null);
});
test('an initial active berry prevents another berry, even of a different selected type',()=>{
  const cycle=new BerryCycle(708);
  assert.equal(cycle.choose(['RazzBerry'],{RazzBerry:10}),null);
  assert.throws(()=>cycle.request(),/Duplicate/);
});
test('only a successful response with the requested active item confirms consumption',()=>{
  const cycle=new BerryCycle();cycle.request();
  assert.deepEqual(cycle.settle(0,701,701),{action:'continue',reason:'applied',consumed:true});
  assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
});
test('an already active response prevents stacking without counting another consumed berry',()=>{
  const cycle=new BerryCycle();cycle.request();
  assert.deepEqual(cycle.settle(2,708,701),{action:'continue',reason:'active_item_exists',consumed:false});
  assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
});
test('breaking out consumes the effect and allows a new berry for the next throw',()=>{
  const cycle=new BerryCycle();cycle.request();cycle.settle(0,701,701);cycle.capture(2);
  assert.equal(cycle.choose(order(),{RazzBerry:1}),'RazzBerry');
  cycle.request();assert.equal(cycle.choose(order(),{RazzBerry:1}),null);
});
test('missing the throw keeps the active effect instead of wasting another berry',()=>{
  const cycle=new BerryCycle();cycle.request();cycle.settle(0,701,701);cycle.capture(4);
  assert.equal(cycle.active,701);assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
});
test('completed encounter results skip that target',()=>{
  const cycle=new BerryCycle();cycle.request();
  assert.deepEqual(cycle.settle(1,0,701),{action:'skip',reason:'encounter_completed',consumed:false});
});
test('no-stock responses do not block the ball or retry within the same throw',()=>{
  const cycle=new BerryCycle();cycle.request();
  assert.equal(cycle.settle(3,0,701).reason,'no_item_in_inventory');
  assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
});
test('invalid-category and ambiguous responses suppress further berries for this encounter',()=>{
  for(const [status,item] of [[4,0],[9,701],[NaN,701],[0,0],[0,708],[0,999],[2,0]]) {
    const cycle=new BerryCycle();cycle.request();
    assert.equal(cycle.settle(status,item,701).consumed,false);cycle.capture(2);
    assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
  }
});
test('network rejection remains suppressed after a breakout because acceptance is unknown',()=>{
  const cycle=new BerryCycle();cycle.request();cycle.reject();cycle.capture(2);
  assert.equal(cycle.choose(order(),{RazzBerry:10}),null);
});
test('initial active items cannot be guessed from malformed input',()=>{
  for(const value of [999,-1,'701',NaN,null])assert.throws(()=>new BerryCycle(value),/Invalid active/);
});
test('every new encounter starts with independent application state',()=>{
  const first=new BerryCycle();first.request();first.reject();
  const second=new BerryCycle();assert.equal(second.choose(order(),{RazzBerry:10}),'RazzBerry');
});
