// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const expected = true;
  mockH5vccExperiments.stubGetFeature(expected);
  let actual = await window.h5vcc.experiments.getFeature();
  assert_equals(actual, expected);
}, 'exercises H5vccExperiments.getFeature()');
