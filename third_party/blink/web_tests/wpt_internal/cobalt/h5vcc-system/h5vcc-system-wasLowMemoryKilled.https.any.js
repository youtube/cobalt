// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");

    const expected = true;
    mockH5vccSystem.stubWasLowMemoryKilled(expected);
    let actual = await window.h5vcc.system.wasLowMemoryKilled();
    assert_equals(actual, expected);

    mockH5vccSystem.stubWasLowMemoryKilled(false);
    actual = await window.h5vcc.system.wasLowMemoryKilled();
    assert_equals(actual, false);

    mockH5vccSystem.stubWasLowMemoryKilled(null);
    await promise_rejects_dom(
        t, 'NotSupportedError', window.h5vcc.system.wasLowMemoryKilled());
}, 'exercises H5vccSystem.wasLowMemoryKilled()');
