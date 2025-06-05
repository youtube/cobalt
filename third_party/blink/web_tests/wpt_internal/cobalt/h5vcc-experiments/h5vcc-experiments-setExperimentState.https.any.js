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
  "experimentIds": [123, 456]
}

h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  let setExperimentStateCalled = false;
  try {
    await window.h5vcc.experiments.setExperimentState(test_experiment_config);
    setExperimentStateCalled = true;
  } finally {
    assert_true(setExperimentStateCalled);
  }
}, 'exercises H5vccExperiments.setExperimentState()');


promise_test(async (t) => {
  const mocks = await import('./resources/mock-h5vcc-experiments.js');
  let interceptor =
    new MojoInterfaceInterceptor(mocks.mockH5vccExperiments.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setExperimentState(test_experiment_config));
}, 'setExperimentState() rejects when unimplemented due to pipe closure');
