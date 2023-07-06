# Cobalt Black Box Test Framework

## Overview

Black Box Test is a place to put End-To-End tests of the Cobalt process. Black
Box Tests can launch, send signals to, and terminate Cobalt using platforms'
app launcher. Black Box Tests can also control Cobalt using JavaScript or
webdriver and can be used to test multiple restarts of the Cobalt process and
data that persists between them. They can also be used to test Cobalt's
interaction with different web server behavior.

### Some examples of things you can test using the Black Box Tests:
  1. Persistent Cookies between consecutive Cobalt sessions.
  2. Basic functionalities after resuming from suspend state.
  3. Re-fetch network requests after network shortage.


## Adding a New Test

  1. Add a python test script in tests/.
     1.  The python script, at a minimum, needs to load your HTML (see step #2)
         and call both `runner.WaitForJSTestsSetup()` and
         `self.assertTrue(runner.JSTestsSucceeded())`. See one of the many python
         scripts in this directory as an example.
  2. Add target web page(s) and associated resources(if any) to testdata/.
     1.  At a minimum, call `setupFinished()` and `onEndTest()` when the test is
         ready to run and when it's completed running respectively. You'll need
         to have loaded `<script src='black_box_js_test_utils.js'></script>` for
         these utility methods to be available.
  3. Add the test name (name of the python test script) to black_box_tests.py
     to automate new test. Add the name to the list of tests requiring
     app launcher support for system signals (e.g. suspend/resume), or the list
     of tests requiring deep link support, or the list of tests that don't.


## Running Tests:

  1. To run all tests:

     `$ python path/to/black_box_tests.py --platform PLATFORM --config CONFIG`

     e.g.
     `$ python path/to/black_box_tests.py --platform linux-x64x11 --config
       devel`


  2. To run an individual test:

     `$ python path/to/black_box_tests.py --platform PLATFORM --config CONFIG
       --test_name TEST_NAME`

     e.g.
     `$ python path/to/black_box_tests.py --platform linux-x64x11 --config devel
       --test_name preload_font`

     You can apply `--gtest_filter` through `--target_params`, e.g.
     ```
       $ python path/to/black_box_tests.py --platform linux-x64x11 --config devel
         --test_name web_platform_tests
         --target_params="--gtest_filter=fetch/*"
     ```

     Note: When declaring a gtest_filter, it will override the filters defined
     in GetWebPlatformTestFilters in configuration.py.

  If you see a HTTPConnection error which eventually
  causes a TimeoutException for the test, you might want
  to add `--server_binding_address localhost` in your
  python command.

## Tests

Each script in tests/ includes one python unittest test case.
In a test script, you can perform any webdriver operations on Cobalt through
BlackBoxCobaltRunner class.


## BlackBoxCobaltRunner

A wrapper around the app launcher. BlackBoxCobaltRunner includes a webdriver
module attached to the app launcher's Cobalt instance after it starts running.
Includes a method(`JSTestsSucceeded()`) to check test result on the JavaScript
side. Call this method to wait for JavaScript test result.
black_box_js_test_utils.js provides some utility functions that are meant to
work with `runner.JSTestsSucceeded()` in the python test scripts. Together,
they allow for test logic to exist in either the python test scripts or
JavaScript test data.

Call `OnEndTest()` to signal test completion on the JavaScript side,
`JSTestsSucceeded()` will react to the signal and return the test status of
JavaScript test logic; another example is that when the python script wants to
wait for some setup steps on JavaScript, call `runner.WaitForJSTestsSetup()`.
Calling `setupFinished()` in JavaScript whenever ready will unblock the wait.


## Test Data

A default local test server will be launched before any unit test starts to
serve the test data in black_box_tests/testdata/. The server's port will be
passed to the app launcher to fetch test data from.

Test data can include the target web page for Cobalt to open and any additional
resources (font file, JavaScripts...).

Tests are free to start their own HTTP servers if the default test server is
inadequate (e.g. The test is testing that Cobalt handles receipt of a specific
HTTP server generated error code properly).
