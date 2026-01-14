// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_tests(async (t, mockH5vccUpdater) => {
  const test_channel = "test_channel";
  await window.h5vcc.updater.setUpdaterChannel(test_channel);
  const result = await window.h5vcc.updater.getUpdaterChannel();
  assert_equals(result, test_channel);
}, 'exercises H5vccUpdater.setUpdaterChannel() and getUpdaterChannel()');

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.updater.getUpdaterChannel());
}, 'getUpdaterChannel() rejects when unimplemented due to pipe closure');
