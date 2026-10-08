const test=require('node:test'),assert=require('node:assert/strict');
const {ownedId,classifyPokemon}=require('../src/pokemon-inspection.cjs');
const normal={id:'18446744073709551615',attack:10,defense:10,stamina:10,shiny:false,favorite:false,egg:false,deployed:false};
test('owned uint64 IDs remain exact strings, never JavaScript numbers',()=>{
  assert.equal(ownedId(normal.id),normal.id);
  for(const id of [Number(normal.id),'18446744073709551616','0','-1','01','1.5',undefined])assert.equal(ownedId(id),null);
});
test('shiny OR exact hundo is retained, including shundo',()=>{
  for(const data of [{shiny:true},{attack:15,defense:15,stamina:15},{shiny:true,attack:15,defense:15,stamina:15}]){
    const result=classifyPokemon({...normal,...data});assert.equal(result.decision,'retain');assert.equal(result.transferAllowed,false);
  }
  assert.equal(classifyPokemon({...normal,attack:15,defense:15,stamina:14}).decision,'review');
});
test('favorites, eggs and deployed Pokemon are retained',()=>{
  for(const key of ['favorite','egg','deployed'])assert.equal(classifyPokemon({...normal,[key]:true}).decision,'retain');
});
test('unknown IVs and unknown booleans cannot become candidates',()=>{
  for(const data of [{attack:16},{defense:-1},{stamina:NaN},{shiny:undefined},{favorite:0},{egg:null},{deployed:undefined}])
    assert.equal(classifyPokemon({...normal,...data}).decision,'unknown');
});
test('review is not transfer eligibility; remaining protections are explicit',()=>{
  const result=classifyPokemon(normal);assert.equal(result.decision,'review');assert.equal(result.transferAllowed,false);
  assert.ok(result.unvalidatedProtections.includes('buddy'));assert.equal(result.ivPercent,66);
});
