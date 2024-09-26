// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Assertion helper functions wrapping the chaijs API. */

import './chai.js';

/**
 * value The value to check.
 * message Additional error message.
 */
export function assertTrue(
    value: boolean, message?: string): asserts value {
  chai.assert.isTrue(value, message);
}

/**
 * value The value to check.
 * message Additional error message.
 */
export function assertFalse(value: boolean, message?: string) {
  chai.assert.isFalse(value, message);
}

/**
 * value1 The first operand.
 * value2 The second operand.
 * message Additional error message.
 */
export function assertGE(
    value1: number, value2: number, message?: string) {
  chai.expect(value1).to.be.at.least(value2, message);
}

/**
 * @param value1 The first operand.
 * @param value2 The second operand.
 * @param message Additional error message.
 */
export function assertGT(value1: number, value2: number,
                         message?: string) {
  chai.assert.isAbove(value1, value2, message);
}

/**
 * @param expected The expected value.
 * @param actual The actual value.
 * @param message Additional error message.
 */
export function assertEquals(
    expected: any, actual: any, message?: string) {
  chai.assert.strictEqual(actual, expected, message);
}

export function assertDeepEquals(expected: any, actual: any,
                                 message?: string) {
  chai.assert.deepEqual(actual, expected, message);
}

/**
 * @param value1 The first operand.
 * @param value2 The second operand.
 * @param message Additional error message.
 */
export function assertLE(value1: number, value2: number,
                         message?: string) {
  chai.expect(value1).to.be.at.most(value2, message);
}

/**
 * @param value1 The first operand.
 * @param value2 The second operand.
 * @param message Additional error message.
 */
export function assertLT(value1: number, value2: number,
                         message?: string) {
  chai.assert.isBelow(value1, value2, message);
}

/**
 * @param expected The expected value.
 * @param actual The actual value.
 * @param message Additional error message.
 */
export function assertNotEquals(expected: any, actual: any,
                                message?: string) {
  chai.assert.notStrictEqual(actual, expected, message);
}

/**
 * @param message Additional error message.
 */
export function assertNotReached(message?: string): never {
  chai.assert.fail(null, null, message);
}

/**
 * @param expectedOrConstructor The expected Error constructor, partial or
 *     complete error message string, or RegExp to test the error message.
 * @param message Additional error message.
 */
export function assertThrows(
    testFunction: () => void,
    expectedOrConstructor?: (ErrorConstructor|string|RegExp),
    message?: string) {
  // The implementation of assert.throws goes like:
  //  function (fn, errt, errs, msg) {
  //    if ('string' === typeof errt || errt instanceof RegExp) {
  //      errs = errt;
  //      errt = null;
  //    }
  //    ...
  // That is, if the second argument is string or RegExp, the type of the
  // exception is not checked: only the error message. This is achieved by
  // partially "shifting" parameters (the "additional error message" is not
  // shifted and will be lost). "Shifting" isn't a thing TS compiler understands, so
  // just cast to string.
  // TODO(crbug/1000989): Refactor this into something that makes sense when
  // tests are actually compiled and we can do that safely.
  if ('string' === typeof expectedOrConstructor) {
    chai.assert.throws(
        testFunction, expectedOrConstructor as string);
  } else {
    chai.assert.throws(
        testFunction, expectedOrConstructor as ErrorConstructor, message);
  }
}

/**
 * Verifies that the contents of the expected and observed arrays match.
 * expected The expected result.
 * actual The actual result.
 */
export function assertArrayEquals(expected: any[], actual: any[]) {
  assertDeepEquals(expected, actual);
}

export function assertStringContains(expected: string, contains: string) {
  chai.expect(expected).to.have.string(contains);
}