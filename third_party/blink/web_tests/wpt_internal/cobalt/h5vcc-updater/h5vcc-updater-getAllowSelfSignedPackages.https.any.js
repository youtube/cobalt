// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  mockH5vccUpdater.setMockAllowSelfSignedPackages(true);
  const result = await window.h5vcc.updater.getAllowSelfSignedPackages();
  assert_true(result);
}, 'exercises H5vccUpdater.getAllowSelfSignedPackages()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'API not supported for this platform.', window.h5vcc.updater.getAllowSelfSignedPackages());
}, 'getAllowSelfSignedPackages() rejects when unimplemented due to pipe closure');
