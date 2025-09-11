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
}, 'H5vccRuntime.onDeepLink:  Should immediately dispatch the initial deep link when the onDeepLink event handler is assigned, if the deep link has not already been consumed.');

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const emptyDeepLink = '';
    mockH5vccRuntime.stubInitialDeepLink(emptyDeepLink);

    // TODO (b/394643877) finish the implementation
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).not.toHaveBeenCalled();
}, 'H5vccRuntime.onDeepLink: No callback triggered when no deep link is present');

h5vcc_runtime_tests(async (t, mockH5vccRuntime) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.runtime, "window.h5vcc.runtime not supported");

    const emptyDeepLink = '';
    mockH5vccRuntime.stubInitialDeepLink(emptyDeepLink);

    // TODO (b/394643877) finish the implementation
    // const mockOnDeepLinkCallback = jest.fn();
    // window.h5vcc.runtime.onDeepLink = mockOnDeepLinkCallback;
    // expect(mockOnDeepLinkCallback).not.toHaveBeenCalled();
    // const fakeDeepLink = 'https://fake.domain.com/tv';
    // mockH5vccRuntime.stubInitialDeepLink(fakeDeepLink);
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledTimes(1);
    // expect(mockOnDeepLinkCallback).toHaveBeenCalledWith({
    //   type: 'deeplink',
    //   url: fakeDeepLink,
    // });
}, 'H5vccRuntime.onDeepLink:  Should dispatch warm-start deep links to the registered onDeepLink callback when they are received.');
