const test=require('node:test'),assert=require('node:assert/strict');
const {StartupGate}=require('../src/startup-gate.cjs');
function observe(gate,start=0){let proof;for(let time=start;time<=start+10000;time+=500)proof=gate.observe(time,true,4)||proof;return proof;}
test('automatic validation requires 10 seconds and stable samples before a one-use permit',()=>{
  const g=new StartupGate('session_A','player_A');assert.equal(g.observe(0,true,4),null);
  for(let t=500;t<10000;t+=500)assert.equal(g.observe(t,true,4),null);
  const proof=g.observe(10000,true,4);assert.equal(proof.samples,21);assert.equal(proof.gameRequestsSent,0);
  assert.equal(g.accept(proof,10000),true);assert.equal(g.accept(proof,10000),false);
});
test('foreground loss, long gaps, backward clock or weak reference count invalidates the permit',()=>{
  for(const scenario of ['focus','gap','clock','references']){
    const g=new StartupGate('session_A','player_A'),proof=observe(g);
    if(scenario==='focus')g.observe(10500,false,4);
    if(scenario==='gap')g.observe(12000,true,4);
    if(scenario==='clock')g.observe(9000,true,4);
    if(scenario==='references')g.observe(10500,true,3);
    assert.equal(g.accept(proof,10500),false);
  }
});
test('wrong account, token or session cannot activate an otherwise valid observation',()=>{
  const g=new StartupGate('session_A','player_A'),proof=observe(g);
  for(const delta of [{playerId:'player_B'},{sessionId:'session_B'},{token:'wrong'}])assert.equal(g.accept({...proof,...delta},10000),false);
  assert.equal(g.accept(proof,10000),true);
});
test('expired proof is not renewed without another full observation window',()=>{
  const g=new StartupGate('session_A','player_A'),proof=observe(g);
  assert.equal(g.accept(proof,16000),false);assert.equal(g.observe(16000,true,4),null);
  assert.equal(g.accept(proof,16000),false);const next=observe(g,16500);assert.notEqual(next.token,proof.token);
});
test('fast samples cannot replace elapsed observation time',()=>{
  const g=new StartupGate('session_A','player_A');for(let t=0;t<10000;t+=10)assert.equal(g.observe(t,true,4),null);
  assert.ok(g.observe(10000,true,4));
});
