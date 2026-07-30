// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const expected = 'https://fake.domain.com/tv';
    mockH5vccRuntime.stubInitialDeepLink(expected);
    let actual = await window.h5vcc.runtime.getInitialDeepLink();
    assert_equals(actual, expected);
}, 'exercises H5vccRuntime.getInitialDeepLink()');

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const fakeInitialDeepLink = 'https://fake.domain.com/tv';
    mockH5vccRuntime.stubInitialDeepLink(fakeInitialDeepLink);
    let first_actual = await window.h5vcc.runtime.getInitialDeepLink();
    assert_equals(first_actual, fakeInitialDeepLink);
    let second_actual = await window.h5vcc.runtime.getInitialDeepLink();
    assert_equals(second_actual, '');
}, 'exercises H5vccRuntime.getInitialDeepLink(), the initial deep link is cleared after retrieving it once.');
