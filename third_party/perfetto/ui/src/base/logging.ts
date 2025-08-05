// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {SCM_REVISION, VERSION} from '../gen/perfetto_version';

export type ErrorHandler = (err: string) => void;

let errorHandler: ErrorHandler = (_: string) => {};

export function assertExists<A>(value: A | null | undefined): A {
  if (value === null || value === undefined) {
    throw new Error('Value doesn\'t exist');
  }
  return value;
}

export function assertTrue(value: boolean, optMsg?: string) {
  if (!value) {
    throw new Error(optMsg ?? 'Failed assertion');
  }
}

export function assertFalse(value: boolean, optMsg?: string) {
  assertTrue(!value, optMsg);
}

export function setErrorHandler(handler: ErrorHandler) {
  errorHandler = handler;
}

export function reportError(err: ErrorEvent|PromiseRejectionEvent|{}) {
  let errLog = '';
  let errorObj = undefined;

  if (err instanceof ErrorEvent) {
    errLog = err.message;
    errorObj = err.error;
  } else if (err instanceof PromiseRejectionEvent) {
    errLog = `${err.reason}`;
    errorObj = err.reason;
  } else {
    errLog = `${err}`;
  }
  if (errorObj !== undefined && errorObj !== null) {
    const errStack = (errorObj as {stack?: string}).stack;
    errLog += '\n';
    errLog += errStack !== undefined ? errStack : JSON.stringify(errorObj);
  }
  errLog += '\n\n';
  errLog += `${VERSION} ${SCM_REVISION}\n`;
  errLog += `UA: ${navigator.userAgent}\n`;

  console.error(errLog, err);
  errorHandler(errLog);
}

// This function serves two purposes.
// 1) A runtime check - if we are ever called, we throw an exception.
// This is useful for checking that code we suspect should never be reached is
// actually never reached.
// 2) A compile time check where typescript asserts that the value passed can be
// cast to the "never" type.
// This is useful for ensuring we exhastively check union types.
export function assertUnreachable(_x: never) {
  throw new Error('This code should not be reachable');
}
