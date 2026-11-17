// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  assert_implements(window.h5vcc, 'window.h5vcc not supported');
  assert_implements(window.h5vcc.system, 'window.h5vcc.system not supported');
  assert_equals(typeof window.h5vcc.system.getPreviousSessionDirtyBitStage,
                'function');
  assert_equals(typeof window.h5vcc.system.promoteSessionDirtyBit, 'function');
  assert_equals(typeof window.h5vcc.system.clearSessionDirtyBit, 'function');

  const stage = window.h5vcc.system.getPreviousSessionDirtyBitStage();
  assert_equals(typeof stage, 'string');
  assert_false(stage instanceof Promise);
  assert_in_array(stage, ['clean', 'early', 'late']);

  assert_equals(mockH5vccSystem.promoteSessionDirtyBitCallCount(), 0);
  window.h5vcc.system.promoteSessionDirtyBit();
  await t.step_wait(
      () => mockH5vccSystem.promoteSessionDirtyBitCallCount() === 1,
      'promoteSessionDirtyBit() should dispatch to H5vccSystem Mojo service');
  assert_equals(window.h5vcc.system.getPreviousSessionDirtyBitStage(), stage);
}, 'h5vcc.system exposes synchronous 2-stage session dirty bit methods');
