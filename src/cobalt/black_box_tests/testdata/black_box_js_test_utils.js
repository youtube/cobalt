// This file provides JavaScript-side environment and test utility functions.
// The foolowing constants and logics are used in communication with the python
// tests. Anyone making changes here should also ensure corresponding changes
// are made in black_box_cobalt_runner.py.

const TEST_STATUS_ELEMENT_NAME = 'black_box_test_status';
const SUCCESS_MESSAGE = 'JavaScript_test_succeeded';
const FAILURE_MESSAGE = 'JavaScript_test_failed';
const SETUP_DONE_MESSAGE = 'JavaScript_setup_done';
const EFFECT_AFTER_VISIBILITY_CHANGE_TIMEOUT_SECONDS = 5;

function notReached() {
  document.body.setAttribute(TEST_STATUS_ELEMENT_NAME, FAILURE_MESSAGE);
}

function assertTrue(result) {
  if (!result){
    notReached();
  }
}

function assertFalse(result) {
  if (result){
    notReached();
  }
}

function assertEqual(expected, result) {
  if (expected !== result) {
    console.log('\n' +
      'Black Box Test Equal Test Assertion failed: \n' +
      'expected: ' + expected + '\n' +
      'but got:  ' + result);
    notReached();
  }
}

function assertNotEqual(expected, result) {
  if (expected === result) {
    console.log('\n' +
      'Black Box Test Unequal Assertion failed: \n' +
      'both are: ' + expected);
    notReached();
  }
}

function onEndTest() {
  if (document.body.getAttribute(TEST_STATUS_ELEMENT_NAME) === FAILURE_MESSAGE) {
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