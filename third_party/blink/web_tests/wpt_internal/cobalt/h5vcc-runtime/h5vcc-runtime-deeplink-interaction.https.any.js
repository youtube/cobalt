// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const fakeDeepLink = 'https://fake.domain.com/tv';
    mockH5vccRuntime.stubInitialDeepLink(fakeDeepLink);

    // TODO (b/394643877) finish the implementation
    // let actual = await window.h5vcc.runtime.getInitialDeepLink();
    // assert_equals(actual, fakeDeepLink);
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).not.toHaveBeenCalled();
}, 'h5vcc.runtime: getInitialDeepLink() consumes the deep link, subsequent onDeepLink handler receives nothing.');

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    // TODO (b/394643877) finish the implementation
    // const fakeDeepLink = 'https://fake.domain.com/tv';
    // mockH5vccRuntime.stubInitialDeepLink(fakeDeepLink);
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledTimes(1);
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledWith({
    //   type: 'deeplink',
    //   url: fakeDeepLink,
    // });
    // let actual = await window.h5vcc.runtime.getInitialDeepLink();
    // assert_equals(actual, '');
}, 'h5vcc.runtime: onDeepLink handler consumes the deep link, subsequent getInitialDeepLink() returns empty string.');
