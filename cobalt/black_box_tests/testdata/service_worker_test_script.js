// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const eventCounts = {};
const countEvent = (fnName, expectedEventCount) => {
  if (fnName in eventCounts) {
    eventCounts[fnName]++;
  } else {
    eventCounts[fnName] = 1;
  }
  if (expectedEventCount) {
    assertEqual(expectedEventCount, eventCounts[fnName])
  }
};

const unregisterAll = () => navigator.serviceWorker.getRegistrations().then(registrations =>
        Promise.all(registrations.map(r => r.unregister())));
setTearDown(unregisterAll);
const err = msg => new Error(msg);
const fail = msg => {
  const userDefinedError = new Error(msg);
  return error => {
    printError(userDefinedError);
    return unregisterAll().then(() => notReached(error));
  };
};
const timeoutId = window.setTimeout(fail('timeout'), 10000);
const done = () => {
  window.clearTimeout(timeoutId);
  console.log('Done!');
  onEndTest();
};

const expectEquals = (expected, actual, msg) => {
  window.assertEqual(expected, actual, new Error(msg || ''));
};

setupFinished();

assertFalse(!!self.clients);
assertTrue(!!navigator.serviceWorker);

navigator.serviceWorker.ready.then(registration => {
  assertTrue(!!registration);
  assertTrue(!!registration.active);
  countEvent('testMain', /*expectedEventCount=*/1);
});

navigator.serviceWorker.oncontrollerchange = event => {
 countEvent('testMain', /*expectedEventCount=*/2);
};

const promiseForEach = (xs, fn) => new Promise((resolve, reject) => {
  let index = -1;
  const doNext = () => {
    if (failed()) {
      reject(new Error(`Tests failed. Current sequence index is ${index}.`));
    }
    index++;
    if (index === xs.length) {
      resolve();
      return;
    }
    fn(xs[index]).then(doNext).catch(reject);
  };
  doNext();
});

const promiseSequence = fns => promiseForEach(fns, fn => fn());

const promiseMap = (xs, fn) => Promise.all(xs.map(fn));

const checkInvalidUrls = () => promiseForEach(
  [
    'http://..:..',
    'arg:service_worker_test_worker.js',
    'http:%2fservice_worker_test_worker.js',
  ],
  url => navigator.serviceWorker.register(url)
      .then(fail(`URL ${url} did not raise error.`))
      .catch(error => {
        assertEqual('TypeError', error.name, `url: ${url}`);
      })
);

const checkInvalidScopes = () => promiseForEach(
  [
    ['service_worker_test_worker.js', {scope: 'arg:/'}, 'TypeError'],
    ['service_worker_test_worker.js', {scope: '/foo'}, 'SecurityError'],
    ['service_worker_test_worker.js', {scope: '/%5c'}, 'TypeError'],
  ],
  args => navigator.serviceWorker.register(args[0], args[1])
      .then(fail(`Scope ${args[1].scope} did not raise error.`))
      .catch(error => {
        assertEqual(args[2], error.name);
      })
);

const checkNotTrustedAndEquivalentJob = () => promiseMap(
  Array.from({length: 15}),
  () => navigator.serviceWorker.register('http://www.example.com/', {scope: '/'})
      .then(fail('Untrusted URL did not raise error'))
      .catch(error => {
        assertEqual('SecurityError', error.name);
      })
);

const checkSuccessfulRegistration = () => Promise.all([
  navigator.serviceWorker.register('service_worker_test_worker.js', {
    scope: '/bar/registration/scope',
  })
  .then(registration => {
    assertTrue(!!registration);
    assertEqual('imports', registration.updateViaCache);
    assertTrue(registration.scope.endsWith('bar/registration/scope'));

    let worker = registration.installing;
    if (worker) {
      worker.postMessage(
          'Registration with scope successful, ' +
          'current worker state is installing.');
    }
    worker = registration.waiting;
    if (worker) {
      worker.postMessage(
          'Registration with scope successful, ' +
          'current worker state is waiting.');
    }
    worker = registration.active;
    if (worker) {
      worker.postMessage(
          'Registration with scope successful, ' +
          'current worker state is active.');
    }

    registration.onupdatefound = event => {
      assertTrue(event.target.scope.endsWith('bar/registration/scope'));
    };

    return navigator.serviceWorker.ready.then(registration => {
      assertTrue(!!registration.active);
      registration.active.postMessage('Registration is active after waiting.');
      assertEqual('activating', registration.active.state);

      return promiseSequence([
        () => promiseMap(
          Array.from({length: 5}),
          () => registration.update()
              .then(registration => {
                assertTrue(!!registration);
              })
        ).catch(fail('getRegistration() failed.')),
        () => navigator.serviceWorker.getRegistration('/bo/gus')
          .then(registration => {
            assertFalse(!!registration);
          })
          .catch(fail('getRegistration() failed.')),
        () => navigator.serviceWorker.getRegistration(
            '/bar/registration/scope/deeper')
          .then(registration => {
            assertTrue(!!registration);
            assertEqual('imports', registration.updateViaCache);
            assertTrue(registration.scope.endsWith(
                'bar/registration/scope'));
          })
          .catch(fail('getRegistration() failed.')),
        () => navigator.serviceWorker.getRegistration('registration')
          .then(registration => {
            assertFalse(!!registration);
          })
          .catch(fail('getRegistration() failed.')),
        () => navigator.serviceWorker.getRegistration()
          .then(registration => {
            assertFalse(!!registration);
          })
          .catch(fail),
        () => navigator.serviceWorker.getRegistration(
            '/bar/registration/scope')
          .then(registration => {
            assertTrue(!!registration);
            assertEqual('imports', registration.updateViaCache);
            assertTrue(registration.scope.endsWith(
                'bar/registration/scope'));
          })
          .catch(fail('getRegistration() failed.')),
        () => registration.unregister()
            .then(success => {
              assertTrue(success);
            })
            .then(() =>
                navigator.serviceWorker.getRegistration('bar/registration/scope'))
            .then(registration => {
              assertFalse(!!registration)
            })
            .catch(fail('getRegistration() failed after unregister.')),
      ]);
    });
  }),
]);

const checkClaimableWorker = () => Promise.all([
  new Promise((resolve, reject) => {
    let messageCount = 0;
    navigator.serviceWorker.onmessage = event => {
      console.log('onmessage: ' + messageCount)
      messageCount++;
      // Expecting two messages:
      //   - when sent a message
      //   - when claimed
      if (messageCount === 2) {
        setTimeout(resolve);
        return;
      }
    };
  }),
  navigator.serviceWorker.ready
    .then(registration => {
      assertTrue(!!registration);
      assertTrue(!!registration.active);
      registration.
      registration.active.postMessage(
          'Registration ready received for claimable worker.');
      return registration.unregister();
    })
    .then(success => {
      assertTrue(success);
    })
    .catch(fail),
  navigator.serviceWorker.register(
      'service_worker_test_claimable.js', {scope: './'})
    .then(registration => {
      let worker = registration.installing;
      if (worker) {
        worker.postMessage(
            'Registration successful, current worker state is ' +
            'installing.');
      }
      worker = registration.waiting;
      if (worker) {
        worker.postMessage(
            'Registration successful, current worker state is ' +
              'waiting.');
      }
      worker = registration.active;
      if (worker) {
        worker.postMessage(
            'Registration successful, current worker state is ' +
            'active.');
      }
      // TODO (b/259734597) : This registration is persisting since it is
      // not unregistered and h5vcc storage clearCache does not correctly clear
      // registrations.
      // TODO (b/259731731) : Adding registration.unregister() causes memory
      // leaks.
    })
    .catch(fail),
]);

promiseSequence([
  unregisterAll,
  checkInvalidUrls,
  checkInvalidScopes,
  checkNotTrustedAndEquivalentJob,
  () => navigator.serviceWorker.register('http://127.0.0.100:2345/')
      .then(fail(`Invalid URL did not raise error.`))
      .catch(error => {
        assertEqual('SecurityError', error.name);
      }),
  () => navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: 'http://127.0.0.100:2345/',
      })
      .then(fail(`Invalid scope did not raise error.`))
      .catch(error => {
        assertEqual('SecurityError', error.name);
      }),
  () => navigator.serviceWorker.register('service_worker_test.html')
      .then(fail(`HTML did not raise error.`))
      .catch(error => {
        assertEqual('SecurityError', error.name);
      }),
  () => navigator.serviceWorker.register('service_worker_test_nonexist.js')
      .then(fail(`Unknown script did not raise error.`))
      .catch(error => {
        assertEqual('NetworkError', error.name);
      }),
  checkSuccessfulRegistration,
  unregisterAll,
  checkClaimableWorker,
]).then(done).catch(fail('Error not caught.'));
