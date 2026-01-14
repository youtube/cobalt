// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_updater_mojo_disconnection_tests(async (t) => {
  return promise_rejects_exactly(
    t, 'Mojo connection error.', window.h5vcc.updater.getRequireNetworkEncryption());
}, 'getRequireNetworkEncryption() rejects when unimplemented due to pipe closure');
