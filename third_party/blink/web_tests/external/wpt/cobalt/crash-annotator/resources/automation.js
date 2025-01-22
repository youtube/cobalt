'use strict';

// These tests use a fake implementation of the CrashAnnotatorService Mojo interface.

let fakeCrashAnnotatorService = undefined;

function crash_annotator_test(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc.crashAnnotator,
                      'missing window.h5vcc.crashAnnotator');
    if (fakeCrashAnnotatorService === undefined) {
      const fakes =
          await import('/resources/chromium/cobalt/fake-crash-annotator-service.js');
          fakeCrashAnnotatorService = fakes.fakeCrashAnnotatorService;
    }
    assert_implements(fakeCrashAnnotatorService, 'missing fakeCrashAnnotatorService');

    fakeCrashAnnotatorService.start();
    try {
      await func(test, fakeCrashAnnotatorService);
    } finally {
      fakeCrashAnnotatorService.stop();
      fakeCrashAnnotatorService.reset();
    }
    }, name, properties);
  }
