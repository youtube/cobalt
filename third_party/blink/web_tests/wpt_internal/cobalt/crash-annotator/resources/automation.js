'use strict';

// These tests use a fake implementation of the CrashAnnotator Mojo interface.

let fakeCrashAnnotatorImpl = undefined;

function crash_annotator_test(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc.crashAnnotator,
                      'missing window.h5vcc.crashAnnotator');
    if (fakeCrashAnnotatorImpl === undefined) {
      const fakes = await import('./fake-crash-annotator-impl.js');
      fakeCrashAnnotatorImpl = fakes.fakeCrashAnnotatorImpl;
    }
    assert_implements(fakeCrashAnnotatorImpl, 'missing fakeCrashAnnotatorImpl');

    fakeCrashAnnotatorImpl.start();
    try {
      await func(test, fakeCrashAnnotatorImpl);
    } finally {
      fakeCrashAnnotatorImpl.stop();
      fakeCrashAnnotatorImpl.reset();
    }
    }, name, properties);
  }
