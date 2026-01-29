// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  const test_index = 1;
  const test_sha256 = "test_sha256";
  mockH5vccUpdater.setMockLibrarySha256(test_index, test_sha256);
  const result = await window.h5vcc.updater.getLibrarySha256(test_index);
  assert_equals(result, test_sha256);
}, 'exercises H5vccUpdater.getLibrarySha256()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'API not supported for this platform.', window.h5vcc.updater.getLibrarySha256(0));
}, 'getLibrarySha256() rejects when unimplemented due to pipe closure');
