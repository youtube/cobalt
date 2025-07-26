// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async () => {
  const result = await new Promise(async resolve => {
    const worker = new SharedWorker(
      "language-model-api-test-shared-worker.js"
    );
    worker.port.onmessage = e => {
      resolve(e.data);
    }
  });

  assert_true(result.success, result.error);
});
