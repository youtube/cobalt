// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.updater, "window.h5vcc.updater not supported");

  // currently stubbed out and not implemented, so test will fail.
  // placeholder value here till the updater plumbing is done.
  const expected = '1';
  mockH5vccUpdater.stubUpdateServerUrl(expected);
  let actual = window.h5vcc.updater.updateServerUrl();
  assert_equals(actual, expected);
}, 'exercises H5vccUpdater.updateServerUrl()');
