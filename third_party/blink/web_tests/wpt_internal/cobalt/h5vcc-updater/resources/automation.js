'use strict';


let mockH5vccUpdater = undefined;

function h5vcc_updater_tests(func, name, properties) {
  promise_test(async t => {
    if (mockH5vccUpdater === undefined) {
      if (isChromiumBased) {
        const mocks = await import('./mock-h5vcc-updater.js');
        mockH5vccUpdater = mocks.mockH5vccUpdater;
      }
    }
    assert_implements(
        mockH5vccUpdater, 'missing mockH5vccUpdater after initialization');

    mockH5vccUpdater.start();
    try {
      await func(test, mockH5vccUpdater);
    } finally {
      mockH5vccUpdater.stop();
      mockH5vccUpdater.reset();
    }
  }, name, properties);
}
