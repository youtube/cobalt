'use strict';


let mockH5vccStorage = undefined;

function h5vcc_storage_tests(func, name, properties) {
  promise_test(async t => {
    if (mockH5vccStorage === undefined) {
      if (isChromiumBased) {
        const mocks = await import('./mock-h5vcc-storage.js');
        mockH5vccStorage = mocks.mockH5vccStorage;
      }
    }
    assert_implements(
        mockH5vccStorage, 'missing mockH5vccStorage after initialization');

    mockH5vccStorage.start();
    try {
      await func(test, mockH5vccStorage);
    } finally {
      mockH5vccStorage.stop();
      mockH5vccStorage.reset();
    }
  }, name, properties);
}
