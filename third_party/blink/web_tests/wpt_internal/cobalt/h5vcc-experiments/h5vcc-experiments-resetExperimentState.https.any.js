// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  let resetExperimentStateCalled = false;
  try {
    await window.h5vcc.experiments.resetExperimentState();
    resetExperimentStateCalled = true;
  } finally {
    assert_true(resetExperimentStateCalled);
  }
}, 'exercises H5vccExperiments.resetExperimentState()');
