// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_metrics_tests(async (t, mockH5vccMetrics) => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.metrics, "window.h5vcc.metrics not supported");

  const expected = false;
  mockH5vccMetrics.stubEnableState(expected);
  let actual = window.h5vcc.metrics.isEnabled();
  assert_equals(actual, expected);
}, 'exercises H5vccMetrics.isEnabled()');
