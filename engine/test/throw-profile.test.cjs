const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {profile,PROFILES}=require('../src/throw-profile.cjs');

test('four requested grades use distinct reticles and preserve the existing great default',()=>{
  assert.deepEqual(Object.keys(PROFILES),['NORMAL','NICE','GREAT','EXCELLENT']);
  assert.equal(profile().reticleSize,0.5);
  for(const [name,grade,reticle] of [['NORMAL',1,1.1],['NICE',2,0.85],['GREAT',3,0.5],['EXCELLENT',4,0.05]]) {
    assert.deepEqual(profile(name),{grade,reticleSize:reticle,hitBullseye:true});
    assert.ok(Object.isFrozen(profile(name)));
  }
});

test('reticles lie inside the independently observed grade intervals, not on their boundaries',()=>{
  for(const [name,min,max] of [['NORMAL',1,1.2],['NICE',0.7,1],['GREAT',0.3,0.7],['EXCELLENT',0,0.080881]])
    assert.ok(profile(name).reticleSize>min&&profile(name).reticleSize<max);
});

test('invalid throw settings do not silently choose another grade',()=>{
  for(const value of [null,0,4,'','excellent','5','__proto__','constructor','EXCELLENT '])
    assert.throws(()=>profile(value),/Invalid throw quality/);
});

test('Android saved values and engine profile keys stay synchronized',()=>{
  const source=fs.readFileSync(path.resolve(__dirname,'../../app/src/main/java/dev/rootcatch/automation/ThrowQuality.java'),'utf8');
  const names=[...source.matchAll(/\b([A-Z]+)\("(?:Normal|Nice|Great|Excellent)"\)/g)].map(match=>match[1]);
  assert.deepEqual(names,Object.keys(PROFILES));
});
