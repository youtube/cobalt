# Cobalt Browser Tests Android Runner

This document describes how to use the `run_android_tests.py` script to execute Cobalt browser tests on an Android device.

## Description

The `run_android_tests.py` script automates the process of setting up and running Cobalt browser tests on a connected Android device. It handles the following steps:

1.  Installing the test APK on the device.
2.  If provided, pushing the local test list file to `/sdcard/Download/test_list.txt` on the device.
3.  Creating a compressed archive of test dependencies.
4.  Pushing the dependencies to the device.
5.  Granting necessary permissions to the test application.
6.  Extracting test dependencies on the device.
7.  Running the tests using `am instrument`.
8.  Pulling the test results to the current directory.
9.  Cleaning up the device by uninstalling the application and removing temporary files, including the pushed test list if applicable.

## Prerequisites

Before running the script, ensure you have the following:

1.  **Android Debug Bridge (adb)**: The `adb` command-line tool must be installed and available in your system's `PATH`. You can verify this by running `adb devices`.
2.  **Connected Device**: An Android device or emulator must be connected and recognized by `adb`.

## How to Run the Script

To execute the tests, run the script with the required arguments.

### Example

```bash
python3 cobalt/testing/browser_tests/integration/run_android_tests.py \
    --apk-path /path/to/your/cobalt_browsertests-debug.apk \
    --build-dir /path/to/your/build/directory \
    --test-list /path/to/your/local/test_list.txt \
    --android-serial="YOUR_DEVICE_SERIAL"
```

## Command-Line Arguments

The script accepts the following arguments:

-   `--apk-path`: (Required) The path to the test APK to be installed.
-   `--build-dir`: (Required) The build directory that contains the test runner script's runtime dependencies.
-   `--test-list`: (Optional) The path to a local file containing a specific list of tests to run. This file will be pushed to `/sdcard/Download/test_list.txt` on the device. If not provided, all tests will be run.
-   `--android-serial`: (Optional) The serial number of the specific Android device to run the tests on.
-   `--gtest_filter`: (Optional) A Google Test-style filter string to run a subset of tests (e.g., `MyTestSuite.MyTest*:-MyTestSuite.MyFlakyTest`). When this argument is provided, the `--test-list` argument will be ignored.
## How to Run the Unit Tests

The script itself has a unit test to verify its behavior. To run the test, execute the following command from the root of the project:

```bash
python3 cobalt/testing/browser_tests/integration/run_android_tests_test.py
```
