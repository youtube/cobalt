// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const { OverrideState } = await import(
    '/gen/cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.m.js');
  const test_feature_name = "test_feature_name";

  mockH5vccExperiments.stubResult(test_feature_name, OverrideState.OVERRIDE_USE_DEFAULT);
  let actual = await window.h5vcc.experiments.getFeature(test_feature_name);
  assert_equals(actual, 'DEFAULT');

  mockH5vccExperiments.stubResult(test_feature_name, OverrideState.OVERRIDE_DISABLE_FEATURE);
  actual = await window.h5vcc.experiments.getFeature(test_feature_name);
  assert_equals(actual, 'DISABLED');

  mockH5vccExperiments.stubResult(test_feature_name, OverrideState.OVERRIDE_ENABLE_FEATURE);
  actual = await window.h5vcc.experiments.getFeature(test_feature_name);
  assert_equals(actual, 'ENABLED');
}, 'exercises H5vccExperiments.getFeature()');
