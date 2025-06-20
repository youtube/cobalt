// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  await window.h5vcc.experiments.resetExperimentState();
  assert_true(mockH5vccExperiments.hasCalledResetExperimentState());
}, 'exercises H5vccExperiments.resetExperimentState()');

promise_test(async (t) => {
  const { H5vccExperiments } = await import(
    '/gen/cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.m.js');
  let interceptor =
    new MojoInterfaceInterceptor(H5vccExperiments.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.experiments.resetExperimentState());
}, 'resetExperimentState() rejects when unimplemented due to pipe closure');
