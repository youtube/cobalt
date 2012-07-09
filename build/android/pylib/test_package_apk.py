# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys

import cmd_helper
import logging
import shutil
import tempfile
from test_package import TestPackage


class TestPackageApk(TestPackage):
  """A helper class for running APK-based native tests.

  Args:
    adb: ADB interface the tests are using.
    device: Device to run the tests.
    test_suite: A specific test suite to run, empty to run all.
    timeout: Timeout for each test.
    rebaseline: Whether or not to run tests in isolation and update the filter.
    performance_test: Whether or not performance test(s).
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool: Name of the Valgrind tool.
    dump_debug_info: A debug_info object.
  """

  APK_DATA_DIR = '/data/user/0/org.chromium.native_test/files/'

  def __init__(self, adb, device, test_suite, timeout, rebaseline,
               performance_test, cleanup_test_files, tool,
               dump_debug_info):
    TestPackage.__init__(self, adb, device, test_suite, timeout,
                         rebaseline, performance_test, cleanup_test_files,
                         tool, dump_debug_info)

  def _CreateTestRunnerScript(self, options):
    tool_wrapper = self.tool.GetTestWrapper()
    if tool_wrapper:
      raise RuntimeError("TestPackageApk does not support custom wrappers.")
    command_line_file = tempfile.NamedTemporaryFile()
    # GTest expects argv[0] to be the executable path.
    command_line_file.write(self.test_suite_basename + ' ' + options)
    command_line_file.flush()
    self.adb.PushIfNeeded(command_line_file.name,
                          '/data/local/tmp/' +
                          'chrome-native-tests-command-line')

  def _GetGTestReturnCode(self):
    return None

  def GetAllTests(self):
    """Returns a list of all tests available in the test suite."""
    self._CreateTestRunnerScript('--gtest_list_tests')
    self.adb.RunShellCommand(
        'am start -n '
        'com.android.chrome.native_tests/'
        'android.app.NativeActivity')
    stdout_file = tempfile.NamedTemporaryFile()
    ret = []
    self.adb.Adb().Pull(TestPackageApk.APK_DATA_DIR + 'stdout.txt',
                        stdout_file.name)
    ret = self._ParseGTestListTests(stdout_file)
    return ret

  def CreateTestRunnerScript(self, gtest_filter, test_arguments):
     self._CreateTestRunnerScript('--gtest_filter=%s %s' % (gtest_filter,
                                                            test_arguments))

  def RunTestsAndListResults(self):
    self.adb.StartMonitoringLogcat(clear=True, logfile=sys.stdout)
    self.adb.RunShellCommand(
        'am start -n '
        'org.chromium.native_test/'
        'org.chromium.native_test.ChromeNativeTestActivity')
    return self._WatchTestOutput(self.adb.GetMonitoredLogCat())

  def StripAndCopyExecutable(self):
    # Always uninstall the previous one (by activity name); we don't
    # know what was embedded in it.
    logging.info('Uninstalling any activity with the test name')
    self.adb.Adb().SendCommand('uninstall org.chromium.native_test',
                               timeout_time=60*5)
    logging.info('Installing new apk')
    self.adb.Adb().SendCommand('install -r ' + self.test_suite_full,
                               timeout_time=60*5)
    logging.info('Install has completed.')

  def _GetTestSuiteBaseName(self):
    """Returns the  base name of the test suite."""
    # APK test suite names end with '-debug.apk'
    return os.path.basename(self.test_suite).rsplit('-debug', 1)[0]
