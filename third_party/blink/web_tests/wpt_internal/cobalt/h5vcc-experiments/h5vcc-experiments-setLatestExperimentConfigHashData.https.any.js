// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const test_config_hash = "test_config_hash";
  await window.h5vcc.experiments.setLatestExperimentConfigHashData(test_config_hash);

  assert_true(mockH5vccExperiments.hasCalledSetLatestExperimentConfigHashData());
  assert_true(mockH5vccExperiments.getStubResult(mockH5vccExperiments.getConfigHash()) === test_config_hash);
}, 'setLatestExperimentConfigHashData() sends expected hash data to browser endpoint');


h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  const test_config_hash = "test_config_hash";
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setLatestExperimentConfigHashData(test_config_hash));
}, 'setLatestExperimentConfigHashData() rejects when unimplemented due to pipe closure');
