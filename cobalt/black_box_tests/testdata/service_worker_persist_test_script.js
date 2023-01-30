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


/**
* @param {KeyboardEvent} event
*/
window.onkeydown = function(event) {
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
        h5vcc.storage.enableCache();
        test_successful_registration();
    } else if (event.key == 1) {
        test_persistent_registration();
    } else if (event.key == 2) {
        test_unregistered_persistent_registration_does_not_exist();
    }
}

function test_successful_registration() {
    console.log('test_successful_registration()');
    navigator.serviceWorker.register('service_worker_test_persisted_worker.js', {
        scope: '/bar/registration/scope',
    }).then(function (registration) {
        console.log('(Expected) Registration Succeeded:',
            registration);
        assertNotEqual(null, registration);
        // The default value for RegistrationOptions.type is
        // 'imports'.
        assertEqual('imports', registration.updateViaCache);
        assertTrue(registration.scope.endsWith(
            'bar/registration/scope'));

        // Check that the registration has an activated worker after
        // some time. The delay used here should be long enough for
        // the service worker to complete activating and have the
        // state 'activated'. It has to be longer than the combined
        // delays in the install or activate event handlers.
        window.setTimeout(function () {
            // Since these events are asynchronous, the service
            // worker can be either of these states.
            console.log(
                'Registration active check after timeout',
                registration);
            assertNotEqual(null, registration.active);
            console.log('Registration active',
                registration.active, 'state:',
                registration.active.state);
            assertEqual('activated',
                registration.active.state);
            onEndTest();
        }, 1000);
    }, function (error) {
        console.log(`(Unexpected): ${error}`, error);
        notReached();
        onEndTest();
    });
}

function test_persistent_registration() {
    console.log("test_persistent_registration()");
    navigator.serviceWorker.getRegistration(
        '/bar/registration/scope')
        .then(function (registration) {
            console.log('(Expected) getRegistration Succeeded:',
                registration);
            assertNotEqual(null, registration);
            assertEqual('imports', registration.updateViaCache);
            assertTrue(registration.scope.endsWith(
                'bar/registration/scope'));

            window.setTimeout(function () {
                console.log(
                    'Registration active check after timeout',
                    registration);
                assertNotEqual(null, registration.active);
                console.log('Registration active',
                    registration.active, 'state:',
                    registration.active.state);
                assertEqual('activated',
                    registration.active.state);

                registration.unregister()
                .then(function (success) {
                    console.log('(Expected) unregister success :',
                        success);
                    // Finally, test getRegistration for the
                    // unregistered scope.
                    navigator.serviceWorker.getRegistration(
                        'bar/registration/scope')
                        .then(function (registration) {
                            assertTrue(null == registration ||
                                undefined == registration);
                            onEndTest();
                        }, function (error) {
                            console.log(`(Unexpected): ${error}`,
                                error);
                            notReached();
                            onEndTest();
                        });
                }, function (error) {
                    console.log('(Unexpected) unregister ' +
                        `${error}`, error);
                    assertIncludes('SecurityError: ', `${error}`);
                    notReached();
                    onEndTest();
                });
            }, 1000);
        }, function (error) {
            console.log(`(Unexpected): ${error}`, error);
            notReached();
            onEndTest();
        });
}

function test_unregistered_persistent_registration_does_not_exist() {
    console.log("test_unregistered_persistent_registration_does_not_exist()");
    navigator.serviceWorker.getRegistration(
        '/bar/registration/scope')
        .then(function (registration) {
            console.log('(Expected) getRegistration Succeeded:',
                registration);
            assertEqual(null, registration);
            onEndTest();
        }, function (error) {
            console.log(`(Unexpected): ${error}`, error);
            notReached();
            onEndTest();
        });
}
