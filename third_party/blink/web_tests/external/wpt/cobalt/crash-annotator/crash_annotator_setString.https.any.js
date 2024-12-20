// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

crash_annotator_test(async (t, fake) => {
  let expectedSetStringResult = true;
  fake.stubResult(expectedSetStringResult);
  let setStringResult =
      await navigator.crashAnnotator.setString("some key", "some val");
  assert_equals(setStringResult, expectedSetStringResult);
}, 'setString() returns value provided by browser endpoint');

crash_annotator_test(async (t, fake) => {
  let key = "some key";
  let value = "some value";
  await navigator.crashAnnotator.setString(key, value);
  assert_equals(fake.getAnnotation(key), value);
}, 'setString() sends expected key and value to browser endpoint');
