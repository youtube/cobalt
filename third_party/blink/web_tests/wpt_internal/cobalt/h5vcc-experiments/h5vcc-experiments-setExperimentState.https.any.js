// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
const test_experiment_config = {
  "features": {
    "fake_feature": true
  },
  "featureParams": {
    "fake_feature_param": 1,
  },
  "activeExperimentConfigData": "test_config_data",
  "latestExperimentConfigHashData": "test_config_hash",
}

// TODO(b/416325838) - Investigate issues with multiple test cases.
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  await window.h5vcc.experiments.setExperimentState(test_experiment_config);
  assert_true(mockH5vccExperiments.hasCalledSetExperimentState());

  assert_true(mockH5vccExperiments.getStubResult('fake_feature'));
  // All feature params are stored as strings and all numbers are 64-bit
  // floating-point value in JS.
  assert_equals(mockH5vccExperiments.getStubResult('fake_feature_param'), "1");
}, 'setExperimentState() sends expected config content to browser endpoint');


h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setExperimentState(test_experiment_config));
}, 'setExperimentState() rejects when unimplemented due to pipe closure');
