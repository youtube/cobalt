// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file provides JavaScript-side environment and test utility functions.
// The following constants and logics are used in communication with the python
// tests. Anyone making changes here should also ensure corresponding changes
// are made in black_box_cobalt_runner.py.

const TEST_STATUS_ELEMENT_NAME = 'black_box_test_status';
const SUCCESS_MESSAGE = 'JavaScript_test_succeeded';
const FAILURE_MESSAGE = 'JavaScript_test_failed';
const SETUP_DONE_MESSAGE = 'JavaScript_setup_done';
const EFFECT_AFTER_VISIBILITY_CHANGE_TIMEOUT_SECONDS = 5;

let tearDown = () => {};
function setTearDown(fn) {
  tearDown = fn;
}

function failed() {
  return document.body.getAttribute(TEST_STATUS_ELEMENT_NAME) === FAILURE_MESSAGE;
}

function printError(error) {
  if (!error) {
    return;
  }
  if (!error.message && !error.stack) {
    console.error(error);
    return;
  }
  if (error.stack) {
    console.error('\n' + error.stack);
    return;
  }
  console.error(error.message);
}

function notReached(error) {
  if (failed()) {
    console.error('Already failed.');
    return;
  }
  if (!error) {
    error = Error('');
  }
  Promise.resolve(tearDown()).then(() => {
    printError(error);
    document.body.setAttribute(TEST_STATUS_ELEMENT_NAME, FAILURE_MESSAGE);
  });
}

function assertTrue(result, msg) {
  if (!result) {
    const errorMessage = '\n' +
      'Black Box Test Assertion failed: \n' +
      'expected: true\n' +
      'but got:  ' + result +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
  }
}

function assertFalse(result) {
  if (result) {
    const errorMessage = '\n' +
      'Black Box Test Assertion failed: \n' +
      'expected: false\n' +
      'but got:  ' + result +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
  }
}

function assertEqual(expected, result, msg) {
  if (expected !== result) {
    const errorMessage = '\n' +
      'Black Box Test Equal Test Assertion failed: \n' +
      'expected: ' + expected + '\n' +
      'but got:  ' + result +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
  }
}

function assertIncludes(expected, result, msg) {
  if (!result || !result.includes(expected)) {
    const errorMessage = '\n' +
      'Black Box Test Equal Test Assertion failed: \n' +
      'expected includes: ' + expected + '\n' +
      'but got:  ' + result +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
  }
}

function assertNotEqual(expected, result, msg) {
  if (expected === result) {
    const errorMessage = '\n' +
      'Black Box Test Unequal Assertion failed: \n' +
      'both are: ' + expected +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
  }
}

function onEndTest() {
  if (failed()) {
    return;
  }
  document.body.setAttribute(TEST_STATUS_ELEMENT_NAME, SUCCESS_MESSAGE);
}

class TimerTestCase {
  constructor(name, ExpectedCallTimes) {
    this.name = name;
    this.ExpectedCallTimes = ExpectedCallTimes;
    this.times = 0;
  }
  called() {
    this.times++;
  }
  verify(InputExpectedTimes = -1) {
    let ExpectedCallTimes = this.ExpectedCallTimes
    if (InputExpectedTimes >= 0) {
      ExpectedCallTimes = InputExpectedTimes
    }
    if (this.times !== ExpectedCallTimes) {
      console.log("Test Error: " + this.name + " is called " + this.times +
        " times, expected: " + ExpectedCallTimes);
      assertEqual(ExpectedCallTimes, this.times)
    }
  }
};

function setupFinished() {
  // Append an element to tell python test scripts that setup steps are done
  // on the JavaScript side.
  let span_ele = document.createElement('span');
  span_ele.id = SETUP_DONE_MESSAGE;
  document.appendChild(span_ele);
}
