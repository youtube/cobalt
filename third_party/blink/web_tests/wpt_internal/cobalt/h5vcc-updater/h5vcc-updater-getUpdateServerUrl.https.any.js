// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  const test_url = "https://example.com/update";
  mockH5vccUpdater.setMockUpdateServerUrl(test_url);
  const result = await window.h5vcc.updater.getUpdateServerUrl();
  assert_equals(result, test_url);
}, 'exercises H5vccUpdater.getUpdateServerUrl()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'API not supported for this platform.', window.h5vcc.updater.getUpdateServerUrl());
}, 'getUpdateServerUrl() rejects when unimplemented due to pipe closure');
