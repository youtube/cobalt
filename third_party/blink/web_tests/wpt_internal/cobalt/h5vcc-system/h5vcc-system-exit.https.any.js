// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_system_tests(() => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");
  assert_implements(window.h5vcc.system.exit, "window.h5vcc.exit not supported");
}, 'checks window.h5vcc.system.exit() is implemented');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  assert_equal(0, mockH5vccSystem.exitCallCount());
  window.h5vcc.system.exit()
  assert_equal(1, mockH5vccSystem.exitCallCount());
}, 'when exit(), exit() is called');
