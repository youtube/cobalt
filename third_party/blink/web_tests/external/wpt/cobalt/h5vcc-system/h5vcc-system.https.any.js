// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_test(async (t, fake) => {
    let fakeAdvertisingId = 'dummy id';
    fake.stubResult(fakeAdvertisingId);
    let result =
        await window.h5vcc.system.getAdvertisingId();
    assert_equals(result, fakeAdvertisingId);
  }, 'getAdvertisingId() returns value provided by browser endpoint');
  
  test(() => {
    assert_equals(window.h5vcc.system.limitAdTracking, false);
}, "H5vccSystem has the right property value");
  