// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  let setExperimentStateCalled = false;
  try {
    await window.h5vcc.experiments.setExperimentState();
    setExperimentStateCalled = true;
  } finally {
    assert_true(setExperimentStateCalled);
  }
}, 'exercises H5vccExperiments.setExperimentState()');


h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  mockH5vccExperiments.simulateNoImplementation();
  // try {
  let experiment_config = {
    "features": {
      "fake_feature": true
    },
    "featureParams": {
      "fake_feature_param": 1,
    },
    "experimentIds": [123, 456]
  }
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setExperimentState(experiment_config));
    // let promise = await window.h5vcc.experiments.setExperimentState();
    // setExperimentStateCalled = true;
  // } finally {
    // assert_true(setExperimentStateCalled);
    // assert_true(Promise.resolve().resolves);
    // window.h5vcc.experiments.stop();
  // }
}, 'setExperimentState() rejects when unimplemented due to pipe closure');
