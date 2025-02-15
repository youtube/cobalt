// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");

  let concealCallCount = 0;
  let stopCallCount = 0;
  const concealOrStopParams = {
    beforeConceal: () => concealCallCount++,
    beforeStop: () => stopCallCount++,
  };

  // Stop.
  mockH5vccSystem.stubUserOnExitStrategy(0);
  assert_true(await window.h5vcc.system.concealOrStop(concealOrStopParams));
  assert_equals(0, concealCallCount);
  assert_equals(1, stopCallCount);

  // Conceal.
  mockH5vccSystem.stubUserOnExitStrategy(1);
  assert_true(await window.h5vcc.system.concealOrStop(concealOrStopParams));
  assert_equals(1, concealCallCount);
  assert_equals(1, stopCallCount);

  // No exit.
  mockH5vccSystem.stubUserOnExitStrategy(1);
  assert_true(await window.h5vcc.system.concealOrStop(concealOrStopParams));
  assert_equals(1, concealCallCount);
  assert_equals(1, stopCallCount);
}, 'exercises H5vccSystem.concealOrStop()');
