const test=require('node:test'),assert=require('node:assert/strict');
const {SessionTransferLedger,transferVerdict}=require('../src/session-transfers.cjs');
const ordinary={id:'18446744073709551614',dexId:25,attack:4,defense:8,stamina:9,
  shiny:false,favorite:false,egg:false,deployed:false,buddy:false,fusion:false,limited:false,pokemonClass:0};
const keep={keepShiny:true,keepHundo:true};
test('default keeps shiny OR 15/15/15 OR both; only ordinary new candidate qualifies',()=>{
  assert.equal(transferVerdict(ordinary,keep).transferAllowed,true);
  for(const fields of [{shiny:true},{attack:15,defense:15,stamina:15},{shiny:true,attack:15,defense:15,stamina:15}])
    assert.equal(transferVerdict({...ordinary,...fields},keep).transferAllowed,false);
});
test('checkboxes are retention filters: disabling one does not disable the other',()=>{
  assert.equal(transferVerdict({...ordinary,shiny:true},{...keep,keepShiny:false}).transferAllowed,true);
  assert.equal(transferVerdict({...ordinary,shiny:true,attack:15,defense:15,stamina:15},{...keep,keepShiny:false}).transferAllowed,false);
  assert.equal(transferVerdict({...ordinary,attack:15,defense:15,stamina:15},{...keep,keepHundo:false}).transferAllowed,true);
});
test('unconditional protections cannot be overridden with checkboxes',()=>{
  for(const name of ['favorite','egg','deployed','buddy','fusion','limited'])
    assert.equal(transferVerdict({...ordinary,[name]:true},{keepShiny:false,keepHundo:false}).transferAllowed,false,name);
  for(const pokemonClass of [1,2,3])assert.equal(transferVerdict({...ordinary,pokemonClass},keep).transferAllowed,false);
});
test('missing, malformed or out-of-range protection data is never transferable',()=>{
  for(const name of Object.keys(ordinary)) {
    const copy={...ordinary};delete copy[name];assert.equal(transferVerdict(copy,keep).transferAllowed,false,name);
  }
  for(const pokemonClass of [-1,4,NaN,undefined,'0'])assert.equal(transferVerdict({...ordinary,pokemonClass},keep).decision,'unknown');
  for(const attack of [-1,16,NaN,undefined,14.5])assert.equal(transferVerdict({...ordinary,attack},keep).decision,'unknown');
});
const session='session_A_123';
function ledger(limit=3,capacity=50000){return new SessionTransferLedger(session,limit,capacity);}
function response(request,fields={}){return {...request,status:1,ownedId:ordinary.id,completedAt:10500,
  freshPromise:true,responseClass:'Holoholo.Rpc.CatchPokemonOutProto',...fields};}
function grant(l,id,encounter='100') {return l.settleCapture(response(l.issueCapture(encounter,10000),{ownedId:id}));}
test('an existing inventory ID without an issued successful capture grants nothing',()=>{
  const l=ledger();assert.equal(l.authorize('22','100'),false);
  assert.equal(l.settleCapture(response({sessionId:session,requestId:'invented',encounterId:'100',sentAt:10000})).accepted,false);
  assert.equal(l.captures.size,0);assert.throws(()=>l.begin('22','100'));
});
test('only exact uint64 owned ID and matching successful encounter grant may release once',()=>{
  const l=ledger();
  for(const status of [0,2,3,4])assert.equal(l.settleCapture(response(l.issueCapture('100',10000),{status})).accepted,false);
  assert.equal(grant(l,ordinary.id).accepted,true);
  assert.equal(l.authorize(ordinary.id,'101'),false);assert.throws(()=>l.begin('100','100'));
  l.begin(ordinary.id,'100');assert.throws(()=>l.begin(ordinary.id,'100'));l.finish(ordinary.id,true);
  assert.equal(grant(l,ordinary.id).accepted,false);assert.equal(l.confirmed,1);
});
test('retained or rejected capture IDs never become eligible again',()=>{
  const l=ledger();grant(l,'23');l.finish('23',false);
  assert.equal(grant(l,'23').accepted,false);assert.equal(l.confirmed,0);
});
test('limit applies to confirmed releases; unconfirmed results do not increase it',()=>{
  const l=ledger(1);grant(l,'23');l.begin('23','100');l.finish('23',true);
  grant(l,'24','101');assert.equal(l.authorize('24','101'),false);assert.throws(()=>l.begin('24','101'));
});
test('Stop clears memory, invalidates grants, and a new session inherits nothing',()=>{
  const l=ledger();const proof=response(l.issueCapture('100',10000),{ownedId:'23'});l.settleCapture(proof);l.close();
  assert.equal(l.authorize('23','100'),false);assert.equal(l.captures.size,0);assert.equal(l.seenOwned.size,0);assert.equal(l.requests.size,0);
  assert.equal(l.settleCapture(proof).accepted,false);assert.throws(()=>l.issueCapture('100',11000));
  const next=new SessionTransferLedger('session_B_123');next.issueCapture('100',10000);
  assert.equal(next.settleCapture(proof).accepted,false);assert.equal(next.authorize('23','100'),false);
});
test('owned ID deduplication, capacity and invalid configuration fail closed',()=>{
  const l=ledger(3,2);grant(l,'1');assert.equal(grant(l,'1').accepted,false);grant(l,'2');
  assert.throws(()=>grant(l,'3'));
  for(const value of [-1,1001,NaN,0.5])assert.throws(()=>ledger(value));
  for(const value of ['',null,'../previous','x'.repeat(97)])assert.throws(()=>new SessionTransferLedger(value));
  for(const value of [-1,0,NaN,0.5])assert.throws(()=>ledger(3,value));
});
test('issued requests are consumed exactly once, including rejected responses',()=>{
  const l=ledger();const req=l.issueCapture('100',10000);assert.throws(()=>l.issueCapture('101',10001));
  const proof=response(req,{status:2});assert.equal(l.settleCapture(proof).accepted,false);
  assert.equal(l.settleCapture({...proof,status:1}).reason,'unissued_or_consumed_request');
  assert.equal(l.captures.size,0);assert.equal(l.requests.size,0);
});
test('wrong encounter/time, cached promise, wrong class, late response and invalid uint64 never authorize',()=>{
  for(const fields of [{encounterId:'101'},{sentAt:9999},{freshPromise:false},{freshPromise:undefined},
    {responseClass:'Result'},{completedAt:9999},{completedAt:28001},{completedAt:NaN},
    {ownedId:'0'},{ownedId:'18446744073709551616'},{ownedId:23}]) {
    const l=ledger(),req=l.issueCapture('100',10000);
    assert.equal(l.settleCapture(response(req,fields)).accepted,false,JSON.stringify(fields));
    assert.equal(l.captures.size,0);assert.equal(l.requests.size,0);
    assert.equal(l.settleCapture(response(req)).accepted,false);
  }
});
test('foreign response cannot authorize or consume a current-session request',()=>{
  const l=ledger(),req=l.issueCapture('100',10000);
  assert.equal(l.settleCapture(response(req,{sessionId:'session_B_123'})).accepted,false);
  assert.equal(l.requests.size,1);assert.equal(l.settleCapture(response(req)).accepted,true);
});

test('abandoned capture permits a new request but never grants transfer scope',()=>{
  const l=ledger(),req=l.issueCapture('100',10000);
  l.abandonCapture(req);
  assert.equal(l.requests.size,0);assert.equal(l.captures.size,0);
  assert.equal(l.settleCapture(response(req)).reason,'unissued_or_consumed_request');
  assert.equal(l.authorize(ordinary.id,'100'),false);
  const next=l.issueCapture('101',11000);assert.equal(next.encounterId,'101');
  assert.throws(()=>l.abandonCapture(req));
  l.close();assert.throws(()=>l.abandonCapture(next));
});

test('a mismatched abandonment cannot consume a valid capture request',()=>{
  const l=ledger(),req=l.issueCapture('100',10000);
  for(const fields of [{sessionId:'session_B_123'},{encounterId:'101'},{sentAt:9999},{requestId:'unknown'}]) {
    assert.throws(()=>l.abandonCapture({...req,...fields}));assert.equal(l.requests.size,1);
  }
  l.abandonCapture(req);assert.equal(l.requests.size,0);
});
