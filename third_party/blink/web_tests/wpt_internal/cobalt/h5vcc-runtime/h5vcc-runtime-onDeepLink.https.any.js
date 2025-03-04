// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const fakeDeepLink = 'https://fake.domain.com/tv';
    mockH5vccRuntime.stubInitialDeepLink(fakeDeepLink);

    // TODO (b/394643877) finish the implementation
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledTimes(1); // Check it was called once
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledWith({
    //   type: 'deeplink',
    //   url: fakeDeepLink,
    // });
}, 'When H5vccRuntime.onDeepLink is set, the initialDeepLink should be dispatched immediately.');

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const emptyDeepLink = '';
    mockH5vccRuntime.stubInitialDeepLink(emptyDeepLink);

    // TODO (b/394643877) finish the implementation
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).not.toHaveBeenCalled();
}, 'If the application has no deep link, setting H5vccRuntime.onDeepLink does not trigger the callback.');
