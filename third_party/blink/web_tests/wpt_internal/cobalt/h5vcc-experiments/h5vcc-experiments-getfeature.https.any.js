// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const expected = true;
  const test_feature_name = "test_feature_name";
  mockH5vccExperiments.stubGetFeature(test_feature_name, expected);
  let actual = await window.h5vcc.experiments.getFeature(test_feature_name);
  assert_equals(actual, expected);
}, 'exercises H5vccExperiments.getFeature()');
