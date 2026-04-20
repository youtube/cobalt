// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const test_config_data = "test_config_data";
  mockH5vccExperiments.stubActiveConfigData(test_config_data);
  let result = await window.h5vcc.experiments.getActiveExperimentConfigData();
  assert_equals(result, test_config_data);
}, 'exercises H5vccExperiments.getActiveExperimentConfigData()');

h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.getActiveExperimentConfigData());
}, 'getActiveExperimentConfigData() rejects when unimplemented due to pipe closure');
