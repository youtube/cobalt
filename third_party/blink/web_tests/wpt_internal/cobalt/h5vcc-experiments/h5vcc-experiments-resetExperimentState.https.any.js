// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  await window.h5vcc.experiments.resetExperimentState();
  assert_true(mockH5vccExperiments.hasCalledResetExperimentState());
}, 'exercises H5vccExperiments.resetExperimentState()');

h5vcc_experiments_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.experiments.resetExperimentState());
}, 'resetExperimentState() rejects when unimplemented due to pipe closure');
