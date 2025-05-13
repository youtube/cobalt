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

h5vcc_experiments_tests(async (t, mockH5vccExperiments) => {
  mockH5vccExperiments.simulateNoImplementation();
  // await promise_rejects_exactly(
  //   t, 'DOMException', window.h5vcc.experiments.resetExperimentState());
  // assert_unreached('Promise should have rejected');
  try {
    await window.h5vcc.experiments.resetExperimentState();
  } finally {
    assert_unreached('Promise should have rejected');
  }
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
}, 'resetExperimentState() rejects when unimplemented due to pipe closure');
