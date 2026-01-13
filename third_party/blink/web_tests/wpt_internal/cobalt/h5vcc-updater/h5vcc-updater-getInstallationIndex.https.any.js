// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  const test_index = 42;
  mockH5vccUpdater.setMockInstallationIndex(test_index);
  const result = await window.h5vcc.updater.getInstallationIndex();
  assert_equals(result, test_index);
}, 'exercises H5vccUpdater.getInstallationIndex()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.updater.getInstallationIndex());
}, 'getInstallationIndex() rejects when unimplemented due to pipe closure');
