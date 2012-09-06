#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Monkey tests on one or more devices."""
import logging
import optparse
import time

from pylib import android_commands
from pylib import python_test_base
from pylib import python_test_caller
from pylib import python_test_sharder
from pylib import test_options_parser
from pylib import test_result


class MonkeyTest(python_test_base.PythonTestBase):
  def __init__(self, test_name, options):
    self.options = options
    super(MonkeyTest, self).__init__(test_name)

  def testMonkey(self):
    start_ms = int(time.time()) * 1000

    # Launch and wait for Chrome to launch.
    self.adb.StartActivity(self.options['package_name'],
                           self.options.pop('activity_name'),
                           wait_for_completion=True,
                           action='android.intent.action.MAIN')

    # Chrome crashes are not always caught by Monkey test runner.
    # Verify Chrome has the same PID before and after the test.
    before_pids = self.adb.ExtractPid(self.options['package_name'])

    # Run the test.
    output = ''
    duration_ms = 0
    if before_pids:
      output = '\n'.join(self.adb.RunMonkey(**self.options))
      duration_ms = int(time.time()) * 1000 - start_ms
      after_pids = self.adb.ExtractPid(self.options['package_name'])

    crashed = (not before_pids or not after_pids
               or after_pids[0] != before_pids[0])
    result = test_result.SingleTestResult(self.qualified_name, start_ms,
                                          duration_ms, log=output)
    results = test_result.TestResults()

    if 'Monkey finished' in output and not crashed:
      results.ok = [result]
    else:
      results.crashed = [result]

    return results


def DispatchPythonTests(options):
  """Dispatches the Monkey tests, sharding it if there multiple devices."""
  logger = logging.getLogger()
  logger.setLevel(logging.DEBUG)

  build_type = options.pop('build_type')
  available_tests = [MonkeyTest('testMonkey', options)]
  attached_devices = android_commands.GetAttachedDevices()
  if not attached_devices:
    raise Exception('You have no devices attached or visible!')

  # Actually run the tests.
  logging.debug('Running monkey tests.')
  available_tests *= len(attached_devices)
  sharder = python_test_sharder.PythonTestSharder(
      attached_devices, 1, available_tests)
  result = sharder.RunShardedTests()
  result.LogFull('Monkey', 'Monkey', build_type)
  result.PrintAnnotation()


def main():
  desc = 'Run the Monkey tests on 1 or more devices.'
  parser = optparse.OptionParser(description=desc)
  test_options_parser.AddBuildTypeOption(parser)
  parser.add_option('--package-name',
                    help='Allowed package.')
  parser.add_option('--activity-name',
                    default='com.google.android.apps.chrome.Main',
                    help='Name of the activity to start [default: %default].')
  parser.add_option('--category',
                    help='A list of allowed categories [default: ""].')
  parser.add_option('--throttle', default=100, type='int',
                    help='Delay between events (ms) [default: %default]. ')
  parser.add_option('--seed', type='int',
                    help='Seed value for pseduo-random generator. Same seed'
                         ' value generates the same sequence of events. Seed is'
                         ' randomized by default.')
  parser.add_option('--event-count', default=10000, type='int',
                    help='Number of events to generate [default: %default].')
  parser.add_option('--verbosity', default=1, type='int',
                    help='Verbosity level [0-3] [default: %default].')
  parser.add_option('--extra-args', default='',
                    help='String of other args to pass to the command verbatim'
                         ' [default: "%default"].')
  (options, args) = parser.parse_args()

  if args:
    parser.error('Unknown arguments: %s' % args)

  if not options.package_name:
    parser.error('Missing package name')

  if options.category:
    options.category = options.category.split(',')

  DispatchPythonTests(vars(options))


if __name__ == '__main__':
  main()
