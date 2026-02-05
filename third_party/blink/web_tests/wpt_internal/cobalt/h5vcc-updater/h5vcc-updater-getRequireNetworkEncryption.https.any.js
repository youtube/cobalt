// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  mockH5vccUpdater.setMockRequireNetworkEncryption(true);
  const result = await window.h5vcc.updater.getRequireNetworkEncryption();
  assert_true(result);
}, 'exercises H5vccUpdater.getRequireNetworkEncryption()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'API not supported for this platform.', window.h5vcc.updater.getRequireNetworkEncryption());
}, 'getRequireNetworkEncryption() rejects when unimplemented due to pipe closure');
