'use strict';

let mockH5vccUpdater = undefined;

function h5vcc_updater_tests(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc.updater,
                      'missing window.h5vcc.updater');
    if (mockH5vccUpdater === undefined) {
      const mocks = await import('./mock-h5vcc-updater.js');
      mockH5vccUpdater = mocks.mockH5vccUpdater;
    }
    assert_implements(mockH5vccUpdater, 'missing mockH5vccUpdater');

    mockH5vccUpdater.start();
    try {
      await func(test, mockH5vccUpdater);
    } finally {
      mockH5vccUpdater.stop();
      mockH5vccUpdater.reset();
    }
  }, name, properties);
}

function h5vcc_updater_mojo_disconnection_tests(func, name, properties) {
  promise_test(async (test) => {
    const { H5vccUpdater } = await import(
      '/gen/cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.m.js');
    let interceptor =
      new MojoInterfaceInterceptor(H5vccUpdater.$interfaceName);
    interceptor.oninterfacerequest = e => e.handle.close();
    interceptor.start();
    try {
      await func(test, interceptor);
    } finally {
      interceptor.stop();
    }
  }, name, properties);
}
