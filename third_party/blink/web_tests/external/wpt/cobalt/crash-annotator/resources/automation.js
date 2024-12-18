'use strict';

// These tests use a fake implementation of the CrashAnnotator Mojo interface.

let fakeCrashAnnotator = undefined;

function crash_annotator_test(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(navigator.crashAnnotator,
                      'missing navigator.crashAnnotator');
    if (fakeCrashAnnotator === undefined) {
      const fakes =
          await import('/resources/chromium/cobalt/fake-crash-annotator.js');
      fakeCrashAnnotator = fakes.fakeCrashAnnotator;
    }
    assert_implements(fakeCrashAnnotator, 'missing fakeCrashAnnotator');

    fakeCrashAnnotator.start();
    try {
      await func(test, fakeCrashAnnotator);
    } finally {
      fakeCrashAnnotator.stop();
      fakeCrashAnnotator.reset();
    }
    }, name, properties);
  }
