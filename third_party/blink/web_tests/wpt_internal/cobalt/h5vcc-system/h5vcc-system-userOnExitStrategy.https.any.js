// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_system_tests(() => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");
  assert_implements(window.h5vcc.system.userOnExitStrategy, "window.h5vcc.system.userOnExitStrategy not supported");
}, 'checks H5vccSystem.userOnExitStrategy is implemented');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyClose();
  // Defined in third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.idl.
  const USER_ON_EXIT_STRATEGY_CLOSE = 0;
  assert_equal(USER_ON_EXIT_STRATEGY_CLOSE, window.h5vcc.system.userOnExitStrategy);
}, 'when user exit strategy is close, userOnExitStrategy returns close');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyMinimize();
  // Defined in third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.idl.
  const USER_ON_EXIT_STRATEGY_MINIMIZE = 1;
  assert_equal(USER_ON_EXIT_STRATEGY_MINIMIZE, window.h5vcc.system.userOnExitStrategy);
}, 'when user exit strategy is minimize, userOnExitStrategy returns minimize');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubUserOnExitStrategyNoexit();
  // Defined in third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.idl.
  const USER_ON_EXIT_STRATEGY_NO_EXIT = 2;
  assert_equal(USER_ON_EXIT_STRATEGY_NO_EXIT, window.h5vcc.system.userOnExitStrategy);
}, 'when user exit strategy is no-exit, userOnExitStrategy returns no-exit');
