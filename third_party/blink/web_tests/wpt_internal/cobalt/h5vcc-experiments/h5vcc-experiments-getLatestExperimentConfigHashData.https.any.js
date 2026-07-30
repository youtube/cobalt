// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const test_config_hash = "test_config_hash";
  mockH5vccExperiments.stubConfigHashData(test_config_hash);
  let result = await window.h5vcc.experiments.getLatestExperimentConfigHashData();
  assert_equals(result, test_config_hash);
}, 'exercises H5vccExperiments.getLatestExperimentConfigHashData()');

h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.getLatestExperimentConfigHashData());
}, 'getLatestExperimentConfigHashData() rejects when unimplemented due to pipe closure');
