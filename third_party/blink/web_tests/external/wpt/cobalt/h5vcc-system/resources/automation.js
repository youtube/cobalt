'use strict';

// These tests use a fake implementation of the H5vccSystem Mojo interface.

let fakeH5vccSystemImpl = undefined;

function h5vcc_system_test(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc.system,
                      'missing window.h5vcc.system');
    if (fakeH5vccSystemImpl === undefined) {
      const fakes =
          await import('/resources/chromium/cobalt/fake-h5vcc-system-impl.js');
          fakeH5vccSystemImpl = fakes.fakeH5vccSystemImpl;
    }
    assert_implements(fakeH5vccSystemImpl, 'missing fakeH5vccSystemImpl');

    fakeH5vccSystemImpl.start();
    try {
      await func(test, fakeH5vccSystemImpl);
    } finally {
      fakeH5vccSystemImpl.stop();
      fakeH5vccSystemImpl.reset();
    }
  }, name, properties);
}
