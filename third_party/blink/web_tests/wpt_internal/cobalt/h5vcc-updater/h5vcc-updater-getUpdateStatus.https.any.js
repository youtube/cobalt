// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  const test_status = "test_status";
  mockH5vccUpdater.setMockUpdateStatus(test_status);
  const result = await window.h5vcc.updater.getUpdateStatus();
  assert_equals(result, test_status);
}, 'exercises H5vccUpdater.getUpdateStatus()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.updater.getUpdateStatus());
}, 'getUpdateStatus() rejects when unimplemented due to pipe closure');
