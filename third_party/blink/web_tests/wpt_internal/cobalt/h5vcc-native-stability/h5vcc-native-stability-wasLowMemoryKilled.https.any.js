// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_native_stability_test(async (t, fake) => {
  assert_implements(window.h5vcc, 'window.h5vcc not supported');
  assert_implements(
    window.h5vcc.nativeStability,
    'window.h5vcc.nativeStability not supported');

  const expected = true;
  fake.stubWasLowMemoryKilled(expected);
  let actual = await window.h5vcc.nativeStability.wasLowMemoryKilled();
  assert_equals(actual, expected);
}, 'exercises H5vccNativeStability.wasLowMemoryKilled()');
