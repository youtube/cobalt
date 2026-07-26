'use strict';


let mockH5vccMetrics = undefined;

function h5vcc_metrics_tests(func, name, properties) {
  promise_test(async t => {
    if (mockH5vccMetrics === undefined) {
      const mocks = await import('./mock-h5vcc-metrics.js');
      mockH5vccMetrics = mocks.mockH5vccMetrics;
    }
    assert_implements(
        mockH5vccMetrics, 'missing mockH5vccMetrics after initialization');

    mockH5vccMetrics.start();
    try {
      await func(test, mockH5vccMetrics);
    } finally {
      mockH5vccMetrics.stop();
      mockH5vccMetrics.reset();
    }
  }, name, properties);
}
