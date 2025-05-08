'use strict';

let mockH5vccExperiments = undefined;

function h5vcc_experiments_tests(func, name, properties) {
  promise_test(async (test) => {
    assert_implements(window.h5vcc.experiments,
                      'missing window.h5vcc.experiments');
    if (mockH5vccExperiments === undefined) {
      const mocks = await import('./mock-h5vcc-experiments.js');
      mockH5vccExperiments = mocks.mockH5vccExperiments;
    }
    assert_implements(mockH5vccExperiments, 'missing mockH5vccExperiments');

    mockH5vccExperiments.start();
    try {
      await func(test, mockH5vccExperiments);
    } finally {
      mockH5vccExperiments.stop();
      mockH5vccExperiments.reset();
    }
    }, name, properties);
  }
