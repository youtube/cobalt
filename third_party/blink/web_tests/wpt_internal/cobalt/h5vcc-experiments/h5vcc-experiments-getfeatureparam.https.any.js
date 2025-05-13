// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  const expected = "FAKE_PARAM_VALUE";
  const test_feature_param_name = "test_feature_param_name";
  mockH5vccExperiments.stubGetFeatureParam(test_feature_param_name, expected);
  console.log('SHOULD CLOSE PIPE IS' + mockH5vccExperiments.should_close_pipe_on_request_);
  let actual = await window.h5vcc.experiments.getFeatureParam(test_feature_param_name);
  assert_equals(actual, expected);
  // try {
  //   await promise_rejects_exactly(
  //     t, e, window.h5vcc.experiments.getFeatureParam(test_feature_param_name));
  // }
  // catch (e) {
  //   // The promise REJECTED. Now we need to know what 'e' is.
  //   console.log('-------------------------------------------');
  //   console.log('Promise rejected! Error object details:');
  //   console.log('  Error Type (typeof):', typeof e);
  //   if (e && typeof e === 'object') {
  //     console.log('  Is DOMException instance?:', e instanceof DOMException);
  //     console.log('  Error Name (e.name):', e.name);
  //     console.log('  Error Message (e.message):', e.message);
  //     console.log('  Full Error Object:', e); // This might be limited in output, but worth a try.
  //     console.log('  Error Stack (e.stack):', e.stack);
  //   } else {
  //     console.log('  Rejected value:', e);
  //   }
  //   console.log('-------------------------------------------');
  //   // Now you can assert based on the actual error you see in the console.
  //   // For example, if it's a 'NetworkError':
  //   // assert_equals(e.name, 'NetworkError', 'Expected NetworkError');
  // }
  // assert_unreached('Promise should have rejected');
}, 'exercises H5vccExperiments.getFeatureParam()');
