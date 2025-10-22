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

// TODO(b/416325838) - Investigate issues with multiple test cases.
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  await window.h5vcc.experiments.setExperimentState(test_experiment_config);
  assert_true(mockH5vccExperiments.hasCalledSetExperimentState());

  // Verify experiment Ids.
  let test_ids = test_experiment_config['experimentIds'];
  for (let i = 0; i < test_ids.length; i++) {
    assert_equals(mockH5vccExperiments.getExperimentIds()[i], test_ids[i]);
  }

  assert_true(mockH5vccExperiments.getFeatureState('fake_feature'));
  // All feature params are stored as strings.
  assert_equals(mockH5vccExperiments.getFeatureState('fake_feature_param'), "1");
}, 'setExperimentState() sends expected config content to browser endpoint');


promise_test(async (t) => {
  const { H5vccExperiments } = await import(
    '/gen/cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.m.js');
  let interceptor =
    new MojoInterfaceInterceptor(H5vccExperiments.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();
  return promise_rejects_exactly(t, 'Mojo connection error.',
    window.h5vcc.experiments.setExperimentState(test_experiment_config));
}, 'setExperimentState() rejects when unimplemented due to pipe closure');
