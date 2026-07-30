// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

const settings = {
  "test_setting_1": "some_setting",
  "test_setting_2": true,
  "test_setting_3": 1,
}

h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  await window.h5vcc.experiments.setFinchParameters(settings);

  assert_equals(mockH5vccExperiments.getStubResult("test_setting_1"), "some_setting");
  assert_true(mockH5vccExperiments.getStubResult("test_setting_2"));
}, 'setFinchParameters() sends expected hash data to browser endpoint');


h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  const test_config_hash = "test_config_hash";
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setLatestExperimentConfigHashData(test_config_hash));
}, 'setFinchParameters() rejects when unimplemented due to pipe closure');
