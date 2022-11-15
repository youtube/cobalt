// This file provides JavaScript-side environment and test utility functions.
// The following constants and logics are used in communication with the python
// tests. Anyone making changes here should also ensure corresponding changes
// are made in black_box_cobalt_runner.py.

const TEST_STATUS_ELEMENT_NAME = 'black_box_test_status';
const SUCCESS_MESSAGE = 'JavaScript_test_succeeded';
const FAILURE_MESSAGE = 'JavaScript_test_failed';
const SETUP_DONE_MESSAGE = 'JavaScript_setup_done';
const EFFECT_AFTER_VISIBILITY_CHANGE_TIMEOUT_SECONDS = 5;

<<<<<<< HEAD   (dc7901 Add missing `youtube-tv` deeplink protocol)
function notReached() {
  // Show the stack that triggered this function.
  try { throw Error('') } catch (error_object) {
    console.log(`${error_object.stack}`);
 }
  document.body.setAttribute(TEST_STATUS_ELEMENT_NAME, FAILURE_MESSAGE);
=======
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
  Promise.resolve(tearDown()).then(() => {
    printError(error);
    document.body.setAttribute(TEST_STATUS_ELEMENT_NAME, FAILURE_MESSAGE);
  });
>>>>>>> CHANGE (121cc3 Clean up the service_worker_test black box test.)
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

function assertEqual(expected, result) {
  if (expected !== result) {
    const errorMessage = '\n' +
      'Black Box Test Equal Test Assertion failed: \n' +
      'expected: ' + expected + '\n' +
<<<<<<< HEAD   (dc7901 Add missing `youtube-tv` deeplink protocol)
      'but got:  ' + result);
    notReached();
=======
      'but got:  ' + result +
      (msg ? `\n${msg}` : '');
    notReached(new Error(errorMessage));
>>>>>>> CHANGE (121cc3 Clean up the service_worker_test black box test.)
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
