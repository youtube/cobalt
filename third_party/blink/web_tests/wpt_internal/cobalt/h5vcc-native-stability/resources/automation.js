'use strict';

// Tests use a fake implementation of H5vccNativeStability Mojo interface.

let fakeH5vccNativeStabilityImpl = undefined;

function h5vcc_native_stability_test(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc, 'missing window.h5vcc');
    assert_implements(
      window.h5vcc.nativeStability,
      'missing window.h5vcc.nativeStability');
    if (fakeH5vccNativeStabilityImpl === undefined) {
      const fakes =
        await import('./fake-h5vcc-native-stability-impl.js');
      fakeH5vccNativeStabilityImpl = fakes.fakeH5vccNativeStabilityImpl;
    }
    assert_implements(
      fakeH5vccNativeStabilityImpl,
      'missing fakeH5vccNativeStabilityImpl');

    fakeH5vccNativeStabilityImpl.start();
    try {
      await func(test, fakeH5vccNativeStabilityImpl);
    } finally {
      fakeH5vccNativeStabilityImpl.stop();
      fakeH5vccNativeStabilityImpl.reset();
    }
  }, name, properties);
}
