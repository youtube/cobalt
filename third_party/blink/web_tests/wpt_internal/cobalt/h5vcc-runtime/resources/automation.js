'use strict';


let mockH5vccRuntime = undefined;

function h5vcc_runtime_tests(func, name, properties) {
  promise_test(async t => {
    if (mockH5vccRuntime === undefined) {
      if (isChromiumBased) {
        const mocks = await import('./mock-h5vcc-runtime.js');
        mockH5vccRuntime = mocks.mockH5vccRuntime;
      }
    }
    assert_implements(
        mockH5vccRuntime, 'missing mockH5vccRuntime after initialization');

    mockH5vccRuntime.start();
    try {
      await func(test, mockH5vccRuntime);
    } finally {
      mockH5vccRuntime.stop();
      mockH5vccRuntime.reset();
    }
  }, name, properties);
}
