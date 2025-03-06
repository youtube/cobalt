// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_system_tests(() => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");
  assert_implements(window.h5vcc.system.onbeforeconceal, "window.h5vcc.system not supported");
  assert_implements(window.h5vcc.system.onbeforeconceal, "window.h5vcc.system not supported");
  assert_implements(window.h5vcc.system.concealOrStop, "window.h5vcc.system not supported");
}, 'checks H5vccSystem.concealOrStop() and associated events are implemented');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyConceal();
  let beforeConcealCalled = false;
  let beforeStopCalled = false;
  window.h5vcc.system.addEventListener('beforeconceal', () => {
    beforeConcealCalled = true;
  }, {once: true});
  window.h5vcc.system.addEventListener('beforestop', () => {
    beforeStopCalled = true;
  }, {once: true});
  assert_true(await window.h5vcc.system.concealOrStop());
  assert_true(beforeConcealCalled);
  assert_false(beforeStopCalled);
}, 'when user exit strategy is conceal, beforeconceal event is fired and concealOrStop() returns true');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyNoexit();
  let beforeConcealCalled = false;
  let beforeStopCalled = false;
  window.h5vcc.system.addEventListener('beforeconceal', () => {
    beforeConcealCalled = true;
  }, {once: true});
  window.h5vcc.system.addEventListener('beforestop', () => {
    beforeStopCalled = true;
  }, {once: true});
  assert_false(await window.h5vcc.system.concealOrStop());
  assert_false(beforeConcealCalled);
  assert_false(beforeStopCalled);
}, 'when user exit strategy is noexit, no events are fired and concealOrStop() returns false');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyStop();
  let beforeConcealCalled = false;
  let beforeStopCalled = false;
  window.h5vcc.system.addEventListener('beforeconceal', () => {
    beforeConcealCalled = true;
  }, {once: true});
  window.h5vcc.system.addEventListener('beforestop', () => {
    beforeStopCalled = true;
  }, {once: true});
  assert_true(await window.h5vcc.system.concealOrStop());
  assert_false(beforeConcealCalled);
  assert_true(beforeStopCalled);
}, 'when user exit strategy is stop, beforestop event is fired and concealOrStop() returns true');
