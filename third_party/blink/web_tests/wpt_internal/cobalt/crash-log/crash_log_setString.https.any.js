// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

crash_log_test(async (t, fake) => {
  let expectedSetStringResult = true;
  fake.stubResult(expectedSetStringResult);
  let setStringResult =
      await window.h5vcc.crashLog.setString("some key", "some val");
  assert_equals(setStringResult, expectedSetStringResult);
}, 'setString() returns value provided by browser endpoint');

crash_log_test(async (t, fake) => {
  let key = "some key";
  let value = "some value";
  await window.h5vcc.crashLog.setString(key, value);
  assert_equals(fake.getAnnotation(key), value);
}, 'setString() sends expected key and value to browser endpoint');

crash_log_test(async (t, fake) => {
  let testValueResult = window.h5vcc.crashLog.testValue;
  assert_equals(testValueResult, true);
}, 'testValue returns value provided by browser endpoint');
