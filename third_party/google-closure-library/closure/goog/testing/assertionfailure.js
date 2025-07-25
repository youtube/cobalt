/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Utilities intended for testing assertion functions.
 */

goog.module('goog.testing.safe.assertionFailure');
goog.setTestOnly();

const asserts = goog.require('goog.asserts');
const testingAsserts = goog.require('goog.testing.asserts');

/**
 * Tests that f raises exactaly one AssertionError and runs f while disabling
 * assertion errors. This is only intended to use in a few test files that is
 * guaranteed that will not affect anything for convenience. It is not intended
 * for broader consumption outside of those test files. We do not want to
 * encourage this pattern.
 *
 * @param {function():*} f function with a failing assertion.
 * @param {string=} opt_message error message the expected error should contain
 * @param {number=} opt_number of time the assertion should throw. Default is 1.
 * @return {*} the return value of f.
 */
exports.withAssertionFailure = function(f, opt_message, opt_number) {
  try {
    if (!opt_number) {
      opt_number = 1;
    }
    var assertions = 0;
    asserts.setErrorHandler(function(e) {
      asserts.assertInstanceof(
          e, asserts.AssertionError, 'A none assertion failure is thrown');
      if (opt_message) {
        testingAsserts.assertContains(opt_message, e.message);
      }
      assertions += 1;
    });
    var result = f();
    asserts.assert(
        assertions == opt_number, '%d assertion failed.', assertions);
    return result;
  } finally {
    asserts.setErrorHandler(asserts.DEFAULT_ERROR_HANDLER);
  }
};
