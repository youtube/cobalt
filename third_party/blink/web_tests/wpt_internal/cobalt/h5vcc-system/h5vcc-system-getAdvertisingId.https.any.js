// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
    assert_implements(window.h5vcc, "window.h5vcc not supported");
    assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");

    const expected = 'fake-ads-id';
    mockH5vccSystem.stubAdvertisingID(expected);
    let actual = await window.h5vcc.system.getAdvertisingId();
    assert_equals(actual, expected);
}, 'exercises H5vccSystem.getAdvertisingId()');
