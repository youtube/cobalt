// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const expected = "FAKE_PARAM_VALUE";
  const test_feature_param_name = "test_feature_param_name";
  mockH5vccExperiments.stubGetFeatureParam(test_feature_param_name, expected);
  let actual = await window.h5vcc.experiments.getFeatureParam(test_feature_param_name);
  assert_equals(actual, expected);
}, 'exercises H5vccExperiments.getFeatureParam()');
