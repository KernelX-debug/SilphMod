'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {harness}=require('./native-harness.cjs');
const {installTrace}=require('../location/location-trace.cjs');
function installed(options={},profile='location_read') {
  const h=harness(options);h.adapter=installTrace(h.runtime,{profile,sdk:options.sdk??29,backtrace:options.backtrace??false},h.report);return h;
}
test('native trace resolves live enum names, exact JNI entry and four independent hooks',()=>{
  const h=installed({},'combined');assert.equal(h.hooks.size,4);
  assert.deepEqual(h.events.find(e=>e.event==='service_status_names').names,{0:'UNDEFINED',1:'RUNNING',2:'FAILED'});
  assert.equal(h.events.find(e=>e.event==='native_entry_validated').artDataOffset,24);
  assert.equal(h.adapter.snapshot().nativeEntryObserved,true);assert.equal(h.adapter.close().passed,true);assert.equal(h.globalCount,0);
});
test('observation preserves every JNI boolean result including unrelated queries',()=>{
  const h=installed({},'mock_observe');assert.equal(h.boolean(1),1);assert.equal(h.boolean(0),0);assert.equal(h.boolean(1,'unrelated'),1);
  assert.equal(h.adapter.snapshot().mockQueries,2);assert.equal(h.adapter.snapshot().mockResultsChanged,0);h.adapter.close();
});
test('explicit treatment only modifies resolved mock methods, across all JNI variants',()=>{
  const h=installed({},'combined');for(const index of [37,38,39])assert.equal(h.boolean(1,'isFromMockProvider',index),0);
  assert.equal(h.boolean(1,'unrelated'),1);assert.equal(h.adapter.snapshot().mockResultsChanged,3);h.adapter.close();
});
test('native callbacks sample provider, status and coordinates without changing their arguments',()=>{
  const h=installed();h.update();const event=h.events.find(e=>e.event==='location_update');
  assert.equal(event.provider,'fused');assert.equal(event.statusOrdinal,1);assert.equal(event.statusName,'RUNNING');
  assert.equal(event.values.latitude,22.1);assert.equal(event.values.longitude,-100.5);
  assert.equal(event.values.elapsedRealtimeNanos,'12345678901234567');assert.equal(event.values.rawMockFlag,true);
  assert.equal(event.coordinateMutation,false);assert.equal(event.locationObjectMutation,false);
  assert.equal(event.serverLocationAcceptanceObserved,false);assert.equal(h.adapter.snapshot().nativeReturns,1);h.adapter.close();
});
test('our getter queries are excluded from query counts and keep the raw flag even in treatment mode',()=>{
  const h=installed({},'combined');h.update();const state=h.adapter.snapshot();
  assert.equal(state.mockQueries,0);assert.equal(state.mockResultsChanged,0);assert.equal(state.skippedOwnQueries,0);assert.equal(state.ownJniTrapsSuppressed,true);
  assert.equal(h.events.find(e=>e.event==='location_update').values.rawMockFlag,true);h.adapter.close();
});
test('null locations record the provider state without calling Location getters',()=>{
  const h=installed();const before=h.calls.length;h.update(2,true);
  const event=h.events.find(e=>e.event==='location_update');assert.equal(event.locationIsNull,true);assert.equal(event.statusName,'FAILED');
  assert.deepEqual(event.values,{});assert.ok(!h.calls.slice(before).includes('DoubleA'));h.adapter.close();
});
test('preexisting exceptions on game callbacks are preserved and sampling is skipped',()=>{
  const h=installed();h.setException(true);const count=h.calls.filter(c=>c==='ExceptionClear').length;h.update();
  assert.ok(h.events.find(e=>e.event==='location_update').sampleErrors.includes('preexisting_jni_exception_preserved'));
  assert.equal(h.calls.filter(c=>c==='ExceptionClear').length,count);h.setException(false);h.adapter.close();
});
test('aliased JNI entry points are hooked once',()=>{
  const h=installed({aliases:true});assert.equal(h.hooks.size,1);h.adapter.close();
});
test('unsupported SDK or entry outside the module is explicit and never guessed',()=>{
  for(const options of [{sdk:35},{outsideModule:true}]) {
    const h=harness(options);assert.throws(()=>installTrace(h.runtime,{profile:'location_read',sdk:options.sdk??29},h.report));
    assert.equal(h.hooks.size,0);assert.equal(h.globalCount,0);assert.ok(h.events.some(e=>e.event==='native_entry_unavailable'));
  }
});
test('partial installation rollback detaches only owned hooks and releases class references',()=>{
  const h=harness({failAttach:2});assert.throws(()=>installTrace(h.runtime,{profile:'combined',sdk:29},h.report),/attach failed/);
  assert.equal(h.hooks.size,0);assert.equal(h.globalCount,0);
});
test('missing mandatory mock method cannot leave active hooks or globals',()=>{
  const h=harness({missingMock:true});assert.throws(()=>installTrace(h.runtime,{profile:'mock_treat',sdk:29},h.report),/required/i);
  assert.equal(h.hooks.size,0);assert.equal(h.globalCount,0);
});
test('only the attached daemon thread is detached and capacity failures abort safely',()=>{
  const h=installed({detached:true});assert.ok(h.calls.includes('AttachDaemon'));assert.ok(h.calls.includes('DetachThread'));h.adapter.close();
  const bad=harness({capacityFail:true});assert.throws(()=>installTrace(bad.runtime,{profile:'combined',sdk:29},bad.report),/capacity/);
  assert.equal(bad.hooks.size,0);assert.equal(bad.globalCount,0);
});
test('preexisting bootstrap exception is not cleared or treated',()=>{
  const h=harness({preexistingException:true});assert.throws(()=>installTrace(h.runtime,{profile:'combined',sdk:29},h.report),/Preexisting/);
  assert.ok(!h.calls.includes('ExceptionClear'));assert.equal(h.hooks.size,0);
});
test('failed detach is reported and does not release classes beneath surviving hooks',()=>{
  const h=installed({failDetach:true},'combined'),closed=h.adapter.close();assert.equal(closed.passed,false);
  assert.equal(closed.remainingHooks,4);assert.ok(h.globalCount>0);assert.equal(h.boolean(1),1);
});
test('summary includes status counts but never represents a server acceptance decision',()=>{
  const h=installed();h.update(0);h.update(1);h.update(2);h.adapter.summary(Date.now(),true);
  const last=h.events.filter(e=>e.event==='trace_summary').at(-1);assert.deepEqual(last.statusCounts,{'0':1,'1':1,'2':1});
  assert.equal(last.serverLocationAcceptanceObserved,false);h.adapter.close();
});

test('JNI preparation resolves framework IDs but installs no hooks or native-entry observer',()=>{
  const h=installed({},'jni_prepare');assert.equal(h.hooks.size,0);assert.equal(h.adapter.snapshot().nativeEntryObserved,false);
  assert.ok(!h.events.some(e=>e.event==='native_entry_validated'));assert.equal(h.adapter.close().passed,true);
});
test('mock-only profiles do not touch the native location entry or invoke its getters',()=>{
  for(const profile of ['mock_observe','mock_treat']) {
    const h=installed({},profile);assert.equal(h.hooks.size,3);assert.equal(h.adapter.snapshot().nativeEntryObserved,false);
    assert.ok(!h.events.some(e=>e.event==='native_entry_validated'));assert.equal(h.boolean(1),profile==='mock_treat'?0:1);h.adapter.close();
  }
});
test('count-only entry callbacks do not perform any diagnostic JNI calls or stack unwinding',()=>{
  const h=installed({backtraceThrows:true},'location_count'),before=h.adapter.snapshot().nativeCalls;
  h.update();assert.equal(h.hooks.size,1);assert.equal(h.adapter.snapshot().nativeCalls,before);
  assert.ok(!h.calls.includes('Backtrace'));assert.deepEqual(h.events.find(e=>e.event==='location_update').values,{});
  assert.equal(h.events.find(e=>e.event==='location_update').samplingReadDisabled,true);h.adapter.close();
});
test('read-only entry profile uses one hook, preserves raw mock values and disables backtraces by default',()=>{
  const h=installed({backtraceThrows:true},'location_read');h.update();assert.equal(h.hooks.size,1);
  assert.equal(h.events.find(e=>e.event==='location_update').values.rawMockFlag,true);assert.ok(!h.calls.includes('Backtrace'));h.adapter.close();
});
test('explicit stack unwinding is isolated and unavailability is reported',()=>{
  const h=installed({backtrace:true,backtraceThrows:true},'location_count');h.update();
  assert.ok(h.calls.includes('Backtrace'));assert.match(h.events.find(e=>e.event==='location_update').backtrace.unavailable,/unavailable/);h.adapter.close();
});
test('minimal cannot accidentally call the JNI installer even through a direct API call',()=>{
  const h=harness();assert.throws(()=>installTrace(h.runtime,{profile:'minimal',sdk:29},h.report));
  assert.equal(h.calls.length,0);assert.equal(h.hooks.size,0);
});
test('failed partial rollback retains its actual surviving hooks on the thrown error',()=>{
  const h=harness({failAttach:2,failDetach:true});
  assert.throws(()=>installTrace(h.runtime,{profile:'combined',sdk:29},h.report),error=>{
    assert.equal(error.traceRollback.passed,false);assert.equal(error.traceRollback.remainingHooks,1);return true;
  });
});
