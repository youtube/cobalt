// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.updater, "window.h5vcc.updater not supported");

  const expected = 'test_url';
  mockH5vccUpdater.stubGetUpdateServerUrl(expected);
  let actual = await window.h5vcc.updater.getUpdateServerUrl();
  assert_equals(actual, expected);
}, 'H5vccUpdater.getUpdateServerUrl() returns the URL set in the mock.');
