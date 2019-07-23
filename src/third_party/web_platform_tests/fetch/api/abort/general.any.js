// META: timeout=long
// META: global=window,worker
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=../request/request-error.js
// META: script=url-polyfill.js
// META: script=gen-async.js

// blob and formData are not supported in current Cobalt implementation
const BODY_METHODS = ['arrayBuffer', /*'blob', 'formData',*/ 'json', 'text'];

// This is used to close connections that weren't correctly closed during the tests,
// otherwise you can end up running out of HTTP connections.
let requestAbortKeys = [];

function abortRequests() {
  const keys = requestAbortKeys;
  requestAbortKeys = [];
  return Promise.all(
    keys.map(key => fetch(`../resources/stash-put.py?key=${key}&value=close`))
  );
}

const hostInfo = get_host_info();
const urlHostname = hostInfo.REMOTE_HOST;

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const fetchPromise = fetch('../resources/data.json', { signal });

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Aborting rejects with AbortError");


// TODO: Cobalt: disabled, mozjs-45 issues
/*
promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const url = new URL('../resources/data.json', location);
  url.hostname = urlHostname;

  const fetchPromise = fetch(url, {
    signal,
    mode: 'no-cors'
  });

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Aborting rejects with AbortError - no-cors");
*/

// Test that errors thrown from the request constructor take priority over abort errors.
// badRequestArgTests is from response-error.js
for (let { args, testName } of badRequestArgTests) {
  promise_test( t => gen_async(function*() {
    try {
      // If this doesn't throw, we'll effectively skip the test.
      // It'll fail properly in ../request/request-error.html
      new Request(...args);
    }
    catch (err) {
      const controller = new AbortController();
      controller.abort();

      // Add signal to 2nd arg
      args[1] = args[1] || {};
      args[1].signal = controller.signal;
      yield promise_rejects(t, new TypeError, fetch(...args));
    }
  }), `TypeError from request constructor takes priority - ${testName}`);
}

test(() => {
  const request = new Request('');
  assert_true(Boolean(request.signal), "Signal member is present & truthy");
  assert_equals(request.signal.constructor, AbortSignal);
}, "Request objects have a signal property");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', { signal });

  assert_true(Boolean(request.signal), "Signal member is present & truthy");
  assert_equals(request.signal.constructor, AbortSignal);
  assert_not_equals(request.signal, signal, 'Request has a new signal, not a reference');
  assert_true(request.signal.aborted, `Request's signal has aborted`);

  const fetchPromise = fetch(request);

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Signal on request object");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', { signal });
  const requestFromRequest = new Request(request);

  const fetchPromise = fetch(requestFromRequest);

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Signal on request object created from request object");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json');
  const requestFromRequest = new Request(request, { signal });

  const fetchPromise = fetch(requestFromRequest);

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Signal on request object created from request object, with signal on second request");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', { signal: new AbortController().signal });
  const requestFromRequest = new Request(request, { signal });

  const fetchPromise = fetch(requestFromRequest);

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Signal on request object created from request object, with signal on second request overriding another");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', { signal });

  const fetchPromise = fetch(request, {method: 'POST'});

  yield promise_rejects(t, "AbortError", fetchPromise);
}), "Signal retained after unrelated properties are overridden by fetch");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', { signal });

  const data = yield fetch(request, { signal: null }).then(r => r.json());
  assert_equals(data.key, 'value', 'Fetch fully completes');
}), "Signal removed by setting to null");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const log = [];

  yield Promise.all([
    fetch('../resources/data.json', { signal }).then(
      () => assert_unreached("Fetch must not resolve"),
      () => log.push('fetch-reject')
    ),
    Promise.resolve().then(() => log.push('next-microtask'))
  ]);

  assert_array_equals(log, ['fetch-reject', 'next-microtask']);
}), "Already aborted signal rejects immediately");

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('../resources/data.json', {
    signal,
    method: 'POST',
    body: 'foo',
    headers: { 'Content-Type': 'text/plain' }
  });

  yield fetch(request).catch(() => {});

  assert_true(request.bodyUsed, "Body has been used");
}), "Request is still 'used' if signal is aborted before fetching");

for (let bodyMethod of BODY_METHODS) {
  promise_test( t => gen_async(function*() {
    const controller = new AbortController();
    const signal = controller.signal;

    const log = [];
    const response = yield fetch('../resources/data.json', { signal });

    controller.abort();

    const bodyPromise = response[bodyMethod]();

    yield Promise.all([
      bodyPromise.catch(() => log.push(`${bodyMethod}-reject`)),
      Promise.resolve().then(() => log.push('next-microtask'))
    ]);

    yield promise_rejects(t, "AbortError", bodyPromise);

    assert_array_equals(log, [`${bodyMethod}-reject`, 'next-microtask']);
  }), `response.${bodyMethod}() rejects if already aborted`);
}

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  const stateKey = token();
  const abortKey = token();
  requestAbortKeys.push(abortKey);
  controller.abort();

  yield fetch(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, { signal }).catch(() => {});

  // I'm hoping this will give the browser enough time to (incorrectly) make the request
  // above, if it intends to.
  yield fetch('../resources/data.json').then(r => r.json());

  const response = yield fetch(`../resources/stash-take.py?key=${stateKey}`);
  const data = yield response.json();

  assert_equals(data, null, "Request hasn't been made to the server");
}), "Already aborted signal does not make request");

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const fetches = [];

  for (let i = 0; i < 3; i++) {
    const abortKey = token();
    requestAbortKeys.push(abortKey);

    fetches.push(
      fetch(`../resources/infinite-slow-response.py?${i}&abortKey=${abortKey}`, { signal })
    );
  }

  for (let fetchPromise of fetches) {
    yield promise_rejects(t, "AbortError", fetchPromise);
  }
}), "Already aborted signal can be used for many fetches");

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;

  yield fetch('../resources/data.json', { signal }).then(r => r.json());

  controller.abort();

  const fetches = [];

  for (let i = 0; i < 3; i++) {
    const abortKey = token();
    requestAbortKeys.push(abortKey);

    fetches.push(
      fetch(`../resources/infinite-slow-response.py?${i}&abortKey=${abortKey}`, { signal })
    );
  }

  for (let fetchPromise of fetches) {
    yield promise_rejects(t, "AbortError", fetchPromise);
  }
}), "Signal can be used to abort other fetches, even if another fetch succeeded before aborting");

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  const stateKey = token();
  const abortKey = token();
  requestAbortKeys.push(abortKey);

  yield fetch(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, { signal });

  const beforeAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
  assert_equals(beforeAbortResult, "open", "Connection is open");

  controller.abort();

  // The connection won't close immediately, but it should close at some point:
  const start = Date.now();

  while (true) {
    // Stop spinning if 10 seconds have passed
    if (Date.now() - start > 10000) throw Error('Timed out');

    const afterAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
    if (afterAbortResult == 'closed') break;
  }
}), "Underlying connection is closed when aborting after receiving response");

// TODO: Cobalt: disabled, cors request raises an early network exception
/*
promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  const stateKey = token();
  const abortKey = token();
  requestAbortKeys.push(abortKey);

  const url = new URL(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, location);
  url.hostname = urlHostname;

  yield fetch(url, {
    signal,
    mode: 'no-cors'
  });

  const stashTakeURL = new URL(`../resources/stash-take.py?key=${stateKey}`, location);
  stashTakeURL.hostname = urlHostname;

  const beforeAbortResult = yield fetch(stashTakeURL).then(r => r.json());
  assert_equals(beforeAbortResult, "open", "Connection is open");

  controller.abort();

  // The connection won't close immediately, but it should close at some point:
  const start = Date.now();

  while (true) {
    // Stop spinning if 10 seconds have passed
    if (Date.now() - start > 10000) throw Error('Timed out');

    const afterAbortResult = yield fetch(stashTakeURL).then(r => r.json());
    if (afterAbortResult == 'closed') break;
  }
}), "Underlying connection is closed when aborting after receiving response - no-cors");
*/

for (let bodyMethod of BODY_METHODS) {
  promise_test( t => gen_async(function*() {
    yield abortRequests();

    const controller = new AbortController();
    const signal = controller.signal;
    const stateKey = token();
    const abortKey = token();
    requestAbortKeys.push(abortKey);

    const response = yield fetch(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, { signal });

    const beforeAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
    assert_equals(beforeAbortResult, "open", "Connection is open");

    const bodyPromise = response[bodyMethod]();

    controller.abort();

    yield promise_rejects(t, "AbortError", bodyPromise);

    const start = Date.now();

    while (true) {
      // Stop spinning if 10 seconds have passed
      if (Date.now() - start > 10000) throw Error('Timed out');

      const afterAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
      if (afterAbortResult == 'closed') break;
    }
  }), `Fetch aborted & connection closed when aborted after calling response.${bodyMethod}()`);
}

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  const stateKey = token();
  const abortKey = token();
  requestAbortKeys.push(abortKey);

  const response = yield fetch(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, { signal });
  const reader = response.body.getReader();

  controller.abort();

  yield promise_rejects(t, "AbortError", reader.read());
  yield promise_rejects(t, "AbortError", reader.closed);

  // The connection won't close immediately, but it should close at some point:
  const start = Date.now();

  while (true) {
    // Stop spinning if 10 seconds have passed
    if (Date.now() - start > 10000) throw Error('Timed out');

    const afterAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
    if (afterAbortResult == 'closed') break;
  }
}), "Stream errors once aborted. Underlying connection closed.");

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;
  const stateKey = token();
  const abortKey = token();
  requestAbortKeys.push(abortKey);

  const response = yield fetch(`../resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`, { signal });
  const reader = response.body.getReader();

  yield reader.read();

  controller.abort();

  yield promise_rejects(t, "AbortError", reader.read());
  yield promise_rejects(t, "AbortError", reader.closed);

  // The connection won't close immediately, but it should close at some point:
  const start = Date.now();

  while (true) {
    // Stop spinning if 10 seconds have passed
    if (Date.now() - start > 10000) throw Error('Timed out');

    const afterAbortResult = yield fetch(`../resources/stash-take.py?key=${stateKey}`).then(r => r.json());
    if (afterAbortResult == 'closed') break;
  }
}), "Stream errors once aborted, after reading. Underlying connection closed.");

promise_test( t => gen_async(function*() {
  yield abortRequests();

  const controller = new AbortController();
  const signal = controller.signal;

  const response = yield fetch(`../resources/empty.txt`, { signal });

  // Read whole response to ensure close signal has sent.
  yield response.clone().text();

  const reader = response.body.getReader();

  controller.abort();

  const item = yield reader.read();

  assert_true(item.done, "Stream is done");
}), "Stream will not error if body is empty. It's closed with an empty queue before it errors.");

/*
// TODO: Cobalt: Test fails when running under unit tests, works fine in browser.
// Likely due to DOMException not being available

promise_test( t => gen_async(function*() {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  let cancelReason;

  const body = new ReadableStream({
    pull(controller) {
      controller.enqueue(new Uint8Array([42]));
    },
    cancel(reason) {
      cancelReason = reason;
    }
  });

  const fetchPromise = fetch('../resources/empty.txt', {
    body, signal,
    method: 'POST',
    headers: {
      'Content-Type': 'text/plain'
    }
  });

  assert_true(!!cancelReason, 'Cancel called sync');
  assert_equals(cancelReason.constructor, DOMException);
  assert_equals(cancelReason.name, 'AbortError');

  yield promise_rejects(t, "AbortError", fetchPromise);

  const fetchErr = yield fetchPromise.catch(e => e);

  assert_equals(cancelReason, fetchErr, "Fetch rejects with same error instance");
}), "Readable stream synchronously cancels with AbortError if aborted before reading");
*/

test(() => {
  const controller = new AbortController();
  const signal = controller.signal;
  controller.abort();

  const request = new Request('.', { signal });
  const requestSignal = request.signal;

  const clonedRequest = request.clone();

  assert_equals(requestSignal, request.signal, "Original request signal the same after cloning");
  assert_true(request.signal.aborted, "Original request signal aborted");
  assert_not_equals(clonedRequest.signal, request.signal, "Cloned request has different signal");
  assert_true(clonedRequest.signal.aborted, "Cloned request signal aborted");
}, "Signal state is cloned");

test(() => {
  const controller = new AbortController();
  const signal = controller.signal;

  const request = new Request('.', { signal });
  const clonedRequest = request.clone();

  const log = [];

  request.signal.addEventListener('abort', () => log.push('original-aborted'));
  clonedRequest.signal.addEventListener('abort', () => log.push('clone-aborted'));

  controller.abort();

  assert_array_equals(log, ['clone-aborted', 'original-aborted'], "Abort events fired in correct order");
  assert_true(request.signal.aborted, 'Signal aborted');
  assert_true(clonedRequest.signal.aborted, 'Signal aborted');
}, "Clone aborts with original controller");
