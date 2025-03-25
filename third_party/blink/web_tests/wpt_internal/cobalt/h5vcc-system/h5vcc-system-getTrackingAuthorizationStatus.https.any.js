// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");

  const expected = "FAKE_STATUS";
  mockH5vccSystem.stubTrackingAuthorizationStatus(expected);
  let actual = await window.h5vcc.system.getTrackingAuthorizationStatus();
  assert_equals(actual, expected);
}, 'exercises H5vccSystem.getTrackingAuthorizationStatus()');
