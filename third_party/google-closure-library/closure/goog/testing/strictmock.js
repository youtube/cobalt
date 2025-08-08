/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview This file defines a strict mock implementation.
 */

goog.setTestOnly('goog.testing.StrictMock');
goog.provide('goog.testing.StrictMock');

goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.testing.Mock');
goog.requireType('goog.testing.MockExpectation');



/**
 * This is a mock that verifies that methods are called in the order that they
 * are specified during the recording phase. Since it verifies order, it
 * follows 'fail fast' semantics. If it detects a deviation from the
 * expectations, it will throw an exception and not wait for verify to be
 * called.
 * @param {Object|Function} objectToMock The object that should be mocked, or
 *    the constructor of an object to mock.
 * @param {boolean=} opt_mockStaticMethods An optional argument denoting that
 *     a mock should be constructed from the static functions of a class.
 * @param {boolean=} opt_createProxy An optional argument denoting that
 *     a proxy for the target mock should be created.
 * @constructor
 * @extends {goog.testing.Mock}
 * @final
 */
goog.testing.StrictMock = function(
    objectToMock, opt_mockStaticMethods, opt_createProxy) {
  'use strict';
  goog.testing.Mock.call(
      this, objectToMock, opt_mockStaticMethods, opt_createProxy);

  /**
   * An array of MockExpectations.
   * @type {!Array<!goog.testing.MockExpectation>}
   * @private
   */
  this.$expectations_ = [];

  /** @private {!Set<!goog.testing.MockExpectation>} */
  this.awaitingExpectations_ = new Set();
};
goog.inherits(goog.testing.StrictMock, goog.testing.Mock);


/** @override */
goog.testing.StrictMock.prototype.$recordExpectation = function() {
  'use strict';
  if (this.$pendingExpectation) {
    this.$expectations_.push(this.$pendingExpectation);
    this.awaitingExpectations_.add(this.$pendingExpectation);
  }
};


/** @override */
goog.testing.StrictMock.prototype.$recordCall = function(name, args) {
  'use strict';
  if (this.$expectations_.length == 0) {
    this.$throwCallException(name, args);
  }

  // If the current expectation has a different name, make sure it was called
  // enough and then discard it. We're through with it.
  var currentExpectation = this.$expectations_[0];
  while (!this.$verifyCall(currentExpectation, name, args)) {
    // This might be an item which has passed its min, and we can now
    // look past it, or it might be below its min and generate an error.
    if (currentExpectation.actualCalls < currentExpectation.minCalls) {
      this.$throwCallException(name, args, currentExpectation);
    }

    this.$expectations_.shift();
    this.awaitingExpectations_.delete(currentExpectation);
    this.maybeFinishedWithExpectations_();
    if (this.$expectations_.length < 1) {
      // Nothing left, but this may be a failed attempt to call the previous
      // item on the list, which may have been between its min and max.
      this.$throwCallException(name, args, currentExpectation);
    }
    currentExpectation = this.$expectations_[0];
  }

  if (currentExpectation.maxCalls == 0) {
    this.$throwCallException(name, args);
  }

  currentExpectation.actualCalls++;
  // If we hit the max number of calls for this expectation, we're finished
  // with it.
  if (currentExpectation.actualCalls == currentExpectation.maxCalls) {
    this.$expectations_.shift();
  }
  if (currentExpectation.actualCalls >= currentExpectation.minCalls) {
    this.awaitingExpectations_.delete(currentExpectation);
    this.maybeFinishedWithExpectations_();
  }

  return this.$do(currentExpectation, args);
};


/** @override */
goog.testing.StrictMock.prototype.$reset = function() {
  'use strict';
  goog.testing.StrictMock.superClass_.$reset.call(this);

  goog.array.clear(this.$expectations_);
  this.awaitingExpectations_.clear();
};


/** @override */
goog.testing.StrictMock.prototype.$waitAndVerify = function() {
  'use strict';
  for (var i = 0; i < this.$expectations_.length; i++) {
    var expectation = this.$expectations_[i];
    goog.asserts.assert(
        !isFinite(expectation.maxCalls) ||
            expectation.minCalls == expectation.maxCalls,
        'Mock expectations cannot have a loose number of expected calls to ' +
            'use $waitAndVerify.');
  }
  var promise = goog.testing.StrictMock.base(this, '$waitAndVerify');
  this.maybeFinishedWithExpectations_();
  return promise;
};

/**
 * @private
 */
goog.testing.StrictMock.prototype.maybeFinishedWithExpectations_ = function() {
  'use strict';
  var unresolvedExpectations =
      this.$expectations_
          .filter(function(expectation) {
            'use strict';
            return expectation.actualCalls < expectation.minCalls;
          })
          .length;
  if (this.waitingForExpectations && !unresolvedExpectations) {
    this.waitingForExpectations.resolve();
  }
};


/** @override */
goog.testing.StrictMock.prototype.$verify = function() {
  'use strict';
  goog.testing.StrictMock.superClass_.$verify.call(this);

  while (this.$expectations_.length > 0) {
    var expectation = this.$expectations_[0];
    if (expectation.actualCalls < expectation.minCalls) {
      this.$throwException(
          'Missing a call to ' + expectation.name + '\nExpected: ' +
          expectation.minCalls + ' but was: ' + expectation.actualCalls);

    } else {
      // Don't need to check max, that's handled when the call is made
      this.$expectations_.shift();
    }
  }
};
