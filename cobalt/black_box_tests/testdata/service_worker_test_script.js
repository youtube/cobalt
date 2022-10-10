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

var expected_event_count = 0;
var expected_event_count_offset = 0;

function total_events() {
    return expected_event_count + expected_event_count_offset;
}

function count_event(expected_value) {
    expected_event_count += 1;
    if (expected_value) {
        assertEqual(expected_value, expected_event_count)
    }
    return expected_event_count;
}

function next_function(next_func) {
    expected_event_count_offset += expected_event_count;
    expected_event_count = 0;
    console.log(next_func.name);
    setTimeout(function () { next_func(); }, 0);
}

assertEqual(undefined, self.clients);

if ('serviceWorker' in navigator) {
    console.log('Starting tests');
    test_main();
}

setupFinished();
// This 7 second delay has to be long enough to guarantee that the test has
// finished and allow for time to see any spurious events that may arrive later.
window.setTimeout(
    () => {
        console.log('Total Events:', total_events())
        assertEqual(44, total_events());
        onEndTest();
        // Exit 3 seconds after ending the test instead of relying on the test
        // runner to force the exit.
        setTimeout(function () { window.close(); }, 3000);
    }, 7000);

function test_main() {
    navigator.serviceWorker.ready.then(function (registration) {
        assertNotEqual(null, registration);
        console.log('(Expected) Registration ready promise',
            registration, 'with active worker ', registration.active);
        assertNotEqual(null, registration.active);
        count_event();
    });

    navigator.serviceWorker.oncontrollerchange = function (event) {
        console.log('Got oncontrollerchange event', event.target);
        count_event(17);
    }

    navigator.serviceWorker.onmessage = function (event) {
        console.log('Got onmessage event', event.target, event.data);
    }

    next_function(test_invalid_script_url);
}

function test_invalid_script_url() {
    navigator.serviceWorker.register('http://..:..'
    ).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log(`(Expected) Test invalid script URL: ${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_script_url_that_is_not_http);
    });
}

function test_script_url_that_is_not_http() {
    navigator.serviceWorker.register('arg:service_worker_test_worker.js'
    ).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test script URL that is not http ' +
            `or https: ${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_script_url_with_escaped_slash);
    });
}

function test_script_url_with_escaped_slash() {
    navigator.serviceWorker.register('http:%2fservice_worker_test_worker.js'
    ).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test script URL with escaped slash:',
            `${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_invalid_scope_url);
    });
}

function test_invalid_scope_url() {
    navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: 'http://..:..',
    }).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log(`(Expected) Test invalid scope URL: ${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_scope_url_that_is_not_http);
    });
}

function test_scope_url_that_is_not_http() {
    navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: 'arg:/',
    }).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test scope URL that is not http ' +
            `or https: ${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_scope_url_that_is_not_allowed);
    });
}

function test_scope_url_that_is_not_allowed() {
    navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: '/foo',
    }).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test scope URL that is not allowed:' +
            `${error}`, error);
        assertIncludes('SecurityError', `${error}`);
        count_event(1);
        next_function(test_scope_url_with_escaped_slash());
    });
}

function test_scope_url_with_escaped_slash() {
    navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: '/%5c',
    }).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test scope URL with escaped slash:',
            `${error}`, error);
        assertIncludes('TypeError', `${error}`);
        count_event(1);
        next_function(test_script_url_not_trusted_and_equivalent_job);
    });
}

function test_script_url_not_trusted_and_equivalent_job() {
    // Repeat a few times to test the 'equivalent job' and finish job
    // logic.
    var repeat_count = 15;
    for (let repeated = 0; repeated < repeat_count; repeated++) {
        navigator.serviceWorker.register('http://www.example.com/', {
            scope: '/',
        }).then(function (registration) {
            console.log('(Unexpected):', registration);
            notReached();
        }, function (error) {
            console.log(`(Expected): ${error}`, error);
            assertIncludes('SecurityError: ', `${error}`);
            if (count_event() >= repeat_count) {
                next_function(test_script_url_and_referrer_origin_not_same);
            }
        });
    }
}

function test_script_url_and_referrer_origin_not_same() {
    navigator.serviceWorker.register('http://127.0.0.100:2345/')
        .then(function (registration) {
            console.log('(Unexpected):', registration);
            notReached();
        }, function (error) {
            console.log(`(Expected): ${error}`, error);
            assertIncludes('SecurityError: ', `${error}`);
            count_event(1);
            next_function(test_scope_url_and_referrer_origin_not_same);
        });
}

function test_scope_url_and_referrer_origin_not_same() {
    navigator.serviceWorker.register('service_worker_test_worker.js', {
        scope: 'http://127.0.0.100:2345/',
    }).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log(`(Expected): ${error}`, error);
        assertIncludes('SecurityError: ', `${error}`);
        count_event(1);
        next_function(test_url_not_javascript_mime_type());
    });
}

function test_url_not_javascript_mime_type() {
    navigator.serviceWorker.register('service_worker_test.html'
    ).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test script URL that is not JavaScript MIME ' +
            `type: ${error}`, error);
        assertIncludes('SecurityError', `${error}`);
        count_event(1);
        next_function(test_url_not_exist);
    });
}

function test_url_not_exist() {
    navigator.serviceWorker.register('service_worker_test_nonexist.js'
    ).then(function (registration) {
        console.log('(Unexpected):', registration);
        notReached();
    }, function (error) {
        console.log('(Expected) Test script URL that does not exist:' +
            `${error}`, error);
        assertIncludes('NetworkError', `${error}`);
        count_event(1);

        // Finally, test a succeeding registration.
        next_function(test_successful_registration());
    });
}

function test_successful_registration() {
    console.log('test_successful_registration()');
    navigator.serviceWorker.register('service_worker_test_worker.js', {
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
        count_event(1);

        worker = registration.installing;
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

        registration.onupdatefound = function (event) {
            console.log('Got onupdatefound event',
                event.target.scope);
            assertTrue(event.target.scope.endsWith(
                'bar/registration/scope'));
        }

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
            registration.active.postMessage(
                'Registration is active after waiting.');
            console.log('Registration active',
                registration.active, 'state:',
                registration.active.state);
            assertEqual('activated',
                registration.active.state);
            count_event(8);
            registration.active.onstatechange =
                function (event) {
                    console.log('Got onstatechange event',
                        event.target.state);
                }

            // Repeat a few times to test the 'equivalent job' and
            // finish job logic.
            for (let repeated = 0; repeated < 5; repeated++) {
                registration.update().then(function (registration) {
                    console.log('(Expected) Registration update ' +
                        'Succeeded:', registration);
                    assertNotEqual(null, registration);
                    count_event();
                }, function (error) {
                    console.log(`(Unexpected): ${error}`, error);
                    notReached();
                });
            }

            registration.unregister()
                .then(function (success) {
                    console.log('(Expected) unregister success :',
                        success);
                    count_event(14);
                    // Finally, test getRegistration for the
                    // unregistered scope.
                    navigator.serviceWorker.getRegistration(
                        'bar/registration/scope')
                        .then(function (registration) {
                            console.log('(Expected) ' +
                                'getRegistration Succeeded: ',
                                registration);
                            assertTrue(null == registration ||
                                undefined == registration);
                            count_event(15);
                        }, function (error) {
                            console.log(`(Unexpected): ${error}`,
                                error);
                            notReached();
                        });

                    test_claimable_worker();
                }, function (error) {
                    console.log('(Unexpected) unregister ' +
                        `${error}`, error);
                    assertIncludes('SecurityError: ', `${error}`);
                    notReached();
                });
        }, 1000);

        // Test getRegistration for a non-registered scope.
        navigator.serviceWorker.getRegistration('/bo/gus')
            .then(function (registration) {
                console.log('(Expected) getRegistration Succeeded:',
                    registration);
                assertTrue(null == registration ||
                    undefined == registration);
                count_event();
            }, function (error) {
                console.log(`(Unexpected): ${error}`, error);
                notReached();
            });

        // Test getRegistration for a deeper registered scope.
        navigator.serviceWorker.getRegistration(
            '/bar/registration/scope/deeper')
            .then(function (registration) {
                console.log('(Expected) getRegistration Succeeded:',
                    registration);
                assertNotEqual(null, registration);
                assertEqual('imports', registration.updateViaCache);
                assertTrue(registration.scope.endsWith(
                    'bar/registration/scope'));
                count_event();
            }, function (error) {
                console.log(`(Unexpected): ${error}`, error);
                notReached();
            });

        // Test getRegistration for a shallower registered scope.
        navigator.serviceWorker.getRegistration('registration')
            .then(function (registration) {
                console.log('(Expected) getRegistration Succeeded:',
                    registration);
                assertTrue(null == registration ||
                    undefined == registration);
                count_event();
            }, function (error) {
                console.log(`(Unexpected): ${error}`, error);
                notReached();
            });

        // Test getRegistration for a non-registered scope.
        navigator.serviceWorker.getRegistration()
            .then(function (registration) {
                console.log('(Expected) getRegistration Succeeded:',
                    registration);
                // TODO(b/234659851): Investigate whether this
                // should return a registration or not, in this case
                // where there is a registration with a scope.
                assertTrue(null == registration ||
                    undefined == registration);
                count_event();
            }, function (error) {
                console.log(`(Unexpected): ${error}`, error);
                notReached();
            });

        // Finally, test getRegistration for a registered scope.
        navigator.serviceWorker.getRegistration(
            '/bar/registration/scope')
            .then(function (registration) {
                console.log('(Expected) getRegistration Succeeded:',
                    registration);
                assertNotEqual(null, registration);
                assertEqual('imports', registration.updateViaCache);
                assertTrue(registration.scope.endsWith(
                    'bar/registration/scope'));
                count_event();
            }, function (error) {
                console.log(`(Unexpected): ${error}`, error);
                notReached();
            });
    }, function (error) {
        console.log(`(Unexpected): ${error}`, error);
        notReached();
    });
}

function test_claimable_worker() {
    console.log('test_claimable_worker()');
    console.log('Adding ready promise.');
    navigator.serviceWorker.ready.then(function (
        registration) {
        assertNotEqual(null, registration);
        console.log('(Expected) Registration ready promise',
            registration, ' with active worker ',
            registration.active);
        assertNotEqual(null, registration.active);
        registration.active.postMessage(
            'Registration ready received for claimable worker.');
        count_event();
        registration.unregister()
            .then(function (success) {
                // Even a claimed registration will successfully
                // unregister because unregistration takes effect after
                // the page is unloaded, so it's not blocked by being
                // the active service worker.
                console.log('(Expected) unregister success :',
                    success);
                count_event(18);
            }, function (error) {
                console.log('(Unexpected) unregister ' +
                    `${error}`, error);
                assertIncludes('SecurityError: ', `${error}`);
                notReached();
            });
        console.log('unregister started.');

    });

    navigator.serviceWorker.register('service_worker_test_claimable.js', {
        scope: './',
    }).then(function (registration) {
        worker = registration.installing;
        if (worker) {
            console.log('claimable worker registered (installing).');
            worker.postMessage(
                'Registration successful, current worker state is ' +
                'installing.');
        }
        worker = registration.waiting;
        if (worker) {
            console.log('claimable worker registered (waiting).');
            worker.postMessage(
                'Registration successful, current worker state is ' +
                'waiting.');
        }
        worker = registration.active;
        if (worker) {
            console.log('claimable worker registered (active).');
            worker.postMessage(
                'Registration successful, current worker state is ' +
                'active.');
        }
    }, function (error) {
        console.log(`(Unexpected): ${error}`, error);
        notReached();
    });
}
