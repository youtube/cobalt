# Test Runner Documentation

The scripts in this folder comprise a cross-platform unit test runner.  The
components of this runner are as follows:

## Components

### 1.) test_runner.py

This script is used to run unit test binaries on multiple platforms.To see a
full list of parameters that can be supplied to the script, run
"python test_runner.py --help".

#### Running Tests:

To run all tests for a given platform, execute the "test_runner.py" script and
provide at minimum the "--platform" and "--config" arguments.  Example:

	python test_runner.py --platform=android-x86 --config=devel

Running this command will run all unit tests for the "devel" build of
the "android-x86" platform.  To specify a device to run the tests
on, provide the "--device_id" argument, as shown below:

  python test_runner.py --platform=android-x86 --config=devel \
    --device_id=emulator-4

#### Running a Single Test:

If you would like to run a single unit test binary and view its output,
you can do so by using the "--target_name" parameter and providing a
test binary name, as shown below:

  python test_runner.py --platform=android-x86 --config=devel \
    --device_id=emulator-4 --target_name=audio_test

#### Building Tests:

You can also use this script to build your unit test binaries before running
them.  To do this, provide the "-b" command line flag.  If you would like to
build the tests and then run them, provide the flags "-br".

### 2.) Master list of test binaries

In your application's "starboard_configuration.py" file, define a variable
called TEST_TARGETS.  It should be a list containing the names of all of
the test binaries that you want the test runner to run. An example is shown
below:

  TEST_TARGETS =[
      "audio_test",
      "bindings_test"
  ]

If your "starboard_configuration.py" file contained this list, then
every platform you support in Starboard would try to run these test
binaries unless they were filtered out as described below.

## Filtering Tests

To filter out tests that you do not want to run for a specific platform,
implement a method within the platform's GYP PlatformConfig class called
"GetTestFilters()".  The PlatformConfig class lives in a
gyp_configuration.py file.  See
[this Linux implementation](../../../linux/x64x11/shared/gyp_configuration.py)
for an example.

The "GetTestFilters()" function should return a list of TestFilter objects,
which can be constructed by importing "./test_filter.py".  To make a
test_filter object, provide the constructor with the test target name, the name
of the actual unit test that the target runs, and optionally, the build
configuration from which the test should be excluded. An example is shown below:

  test_filter.TestFilter('math_test', 'Vector3dTest.IsZero', 'debug')

If a configuration is not provided, then the test will be exluded from all
configurations.

To filter out all tests for a particular target, provide
"test_filter.FILTER_ALL" as the test name.

To disable unit testing for all targets and all configurations, return a list
containing "test_filter.DISABLE_TESTING".

## Environment Variables

If a platform requires extra environment variables in order to run tests
properly, implement a method called "GetTestEnvVariables()" in the same
PlatformConfig mentioned above.  There is an example of this method in
the provided Linux implementation.  The method should return a dictionary that
maps test binary names to dictionaries of environment variables that they need.
Example:

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'crypto_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'net_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'}
    }