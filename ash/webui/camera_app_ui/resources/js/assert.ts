// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Verify |condition| is truthy and return |condition| if so.
 *
 * @param condition A condition to check for truthiness.  Note that this
 *     may be used to test whether a value is defined or not, and we don't want
 *     to force a cast to Boolean.
 * @param optMessage A message to show on failure.
 */
export function assert(
    condition: boolean, optMessage?: string): asserts condition {
  if (!condition) {
    let message = 'Assertion failed';
    if (optMessage) {
      message = message + ': ' + optMessage;
    }
    throw new Error(message);
  }
}

/**
 * Call this from places in the code that should never be reached.
 *
 * This code should only be hit in the case of serious programmer error or
 * unexpected input.
 *
 * @example
 *   Handling all the values of enum with a switch() like this:
 *   ```
 *     function getValueFromEnum(enum) {
 *       switch (enum) {
 *         case ENUM_FIRST_OF_TWO:
 *           return first
 *         case ENUM_LAST_OF_TWO:
 *           return last;
 *       }
 *       assertNotReached();
 *       return document;
 *     }
 *   ```
 * @param optMessage A message to show when this is hit.
 */
export function assertNotReached(optMessage = 'Unreachable code hit'): never {
  assert(false, optMessage);
}

/**
 * @param value The value to check.
 * @param ctor A user-defined constructor.
 * @param optMessage A message to show when this is hit.
 */
export function assertInstanceof<T>(
    value: unknown,
    // "unknown" doesn't work well here if the constructor have overloads with
    // different numbers of argument and strictNullChecks on.
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    ctor: new (...args: any[]) => T,
    optMessage?: string,
    ): T {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (!(value instanceof ctor)) {
    assertNotReached(
        optMessage ??
        'Value ' + value + ' is not a[n] ' + (ctor.name || typeof ctor));
  }
  return value;
}

/**
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertString(value: unknown, optMessage?: string): string {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'string') {
    assertNotReached(optMessage ?? 'Value ' + value + ' is not a string');
  }
  return value;
}

/**
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertNumber(value: unknown, optMessage?: string): number {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'number') {
    assertNotReached(optMessage ?? 'Value ' + value + ' is not a number');
  }
  return value;
}

/**
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertBoolean(value: unknown, optMessage?: string): boolean {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'boolean') {
    assertNotReached(optMessage ?? 'Value ' + value + ' is not a boolean');
  }
  return value;
}

/**
 * Verify that the value is not null or undefined.
 *
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertExists<T>(
    value: T|null|undefined, optMessage?: string): T {
  if (value === null || value === undefined) {
    assertNotReached(optMessage ?? `Value is ${value}`);
  }
  return value;
}
