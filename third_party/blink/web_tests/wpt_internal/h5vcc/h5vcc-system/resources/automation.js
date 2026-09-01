'use strict';


let mockH5vccSystem = undefined;

function h5vcc_system_tests(func, name, properties) {
  promise_test(async t => {
    if (mockH5vccSystem === undefined) {
      if (isChromiumBased) {
        const mocks = await import('./mock-h5vcc-system.js');
        mockH5vccSystem = mocks.mockH5vccSystem;
      }
    }
    assert_implements(
        mockH5vccSystem, 'missing mockH5vccSystem after initialization');

    mockH5vccSystem.start();
    try {
      await func(test, mockH5vccSystem);
    } finally {
      mockH5vccSystem.stop();
      mockH5vccSystem.reset();
    }
  }, name, properties);
}

