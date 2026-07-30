// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  await window.h5vcc.updater.setAllowSelfSignedPackages(true);
  const result = await window.h5vcc.updater.getAllowSelfSignedPackages();
  assert_true(result);
}, 'exercises H5vccUpdater.setAllowSelfSignedPackages()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'API not supported for this build configuration. Enabled for sideloading only.',
    window.h5vcc.updater.setAllowSelfSignedPackages(true));
}, 'setAllowSelfSignedPackages() rejects when unimplemented due to pipe closure');
