// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

crash_annotator_test(async (t, fake) => {
  let expected_result = true;
  fake.stubResult(expected_result);
  let result = await navigator.crashAnnotator.setString("some key", "some val");
  assert_equals(result, expected_result);
}, 'setString() returns value provided by browser endpoint');
