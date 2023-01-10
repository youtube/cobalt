// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

/**
* @param {KeyboardEvent} event
*/

const unregisterAll = () => navigator.serviceWorker.getRegistrations().then(registrations =>
  Promise.all(registrations.map(r => r.unregister())));
setTearDown(unregisterAll);

function TEST(test) {
  console.log('[ RUN', test.name, ']');
  test();
}

window.onkeydown = function (event) {
  assertEqual(undefined, self.clients);

  if (!('serviceWorker' in navigator)) {
    console.log("serviceWorker not in navigator, ending test");
    onEndTest();
  }

  navigator.serviceWorker.ready.then(function (registration) {
    assertNotEqual(null, registration);
    console.log('(Expected) Registration ready promise',
      registration, 'with active worker ', registration.active);
    assertNotEqual(null, registration.active);
  });

  navigator.serviceWorker.oncontrollerchange = function (event) {
    console.log('Got oncontrollerchange event', event.target);
  }

  navigator.serviceWorker.onmessage = function (event) {
    console.log('Got onmessage event', event.target, event.data);
  }

  if (event.key == 0) {
    console.log('Keydown 0');
    TEST(SuccessfulRegistration);
  } else if (event.key == 1) {
    console.log('Keydown 1');
    TEST(SuccessfulActivation);
  } else if (event.key == 2) {
    console.log('Keydown 2');
    unregisterAll().then(() => onEndTest());
 }
}

function SuccessfulRegistration() {
  navigator.serviceWorker.register('service_worker_controller_activation_test_worker.js', {
    scope: '/testdata',
  }).then(function (registration) {
    console.log('(Expected) Registration Succeeded:',
      registration);
    assertNotEqual(null, registration);
    // The default value for RegistrationOptions.type is
    // 'imports'.
    assertEqual('imports', registration.updateViaCache);
    assertTrue(registration.scope.endsWith('/testdata'));

    // Check that the registration has an activated worker after
    // some time. The delay used here should be long enough for
    // the service worker to complete activating and have the
    // state 'activated'. It has to be longer than the combined
    // delays in the install or activate event handlers.
    window.setTimeout(function () {
      // Since these events are asynchronous, the service
      // worker can be either of these states.
      assertNotEqual(null, registration.active);
      registration.active.postMessage('Registration is active after waiting.');
      assertEqual('activated',
        registration.active.state);
      assertNotEqual(null, navigator.serviceWorker.controller);
      assertEqual('activated',
        navigator.serviceWorker.controller.state);
      if (failed()) window.close();
      console.log('Reload page');
      window.location.href =
        window.location.origin + window.location.pathname +
        '?result=SuccessfulRegistration';
    }, 1000);
  }, function (error) {
    console.log(`(Unexpected): ${error}`, error);
    notReached();
    onEndTest();
  });
}

function SuccessfulActivation() {
  navigator.serviceWorker.getRegistration().then(registration => {
    assertTrue(!!registration);
    assertEqual('imports', registration.updateViaCache);
    assertNotEqual(null, registration.active);
    assertEqual('activated', registration.active.state);

    if (failed()) window.close();
    console.log('Reload page');
    window.location.href =
      window.location.origin + window.location.pathname +
      '?result=Success';
  })
    .catch(error => {
      console.log(`(Unexpected): ${error}`, error);
      notReached();
      onEndTest();
    });
  assertNotEqual(null, navigator.serviceWorker.controller);
  assertEqual('activated',
    navigator.serviceWorker.controller.state);
}

var search = document.createElement('div');
search.id = window.location.search.replace(/\?result=/, 'Result');
document.body.appendChild(search);

setupFinished();
