// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js
h5vcc_storage_tests(() => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.storage, "window.h5vcc.storage not supported");
  assert_implements(window.h5vcc.storage.flush, "window.h5vcc.storage.flush not supported");
}, 'checks window.h5vcc.storage.flush() is implemented');

h5vcc_storage_tests(async (t, mockH5vccStorage) => {
  assert_equal(0, mockH5vccStorage.flushCallCount());
  window.h5vcc.storage.flush();
  assert_equal(1, mockH5vccStorage.flushCallCount());
}, 'when flush(), flush() is called');
