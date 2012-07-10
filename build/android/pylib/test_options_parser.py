# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses options for the instrumentation tests."""

import os
import optparse


def CreateTestRunnerOptionParser(usage=None, default_timeout=60):
  """Returns a new OptionParser with arguments applicable to all tests."""
  option_parser = optparse.OptionParser(usage=usage)
  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=default_timeout)
  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true',
                           default=False)
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  profilers = ['activitymonitor', 'chrometrace', 'dumpheap', 'smaps',
               'traceview']
  option_parser.add_option('--profiler', dest='profilers', action='append',
                           choices=profilers,
                           help='Profiling tool to run during test. '
                           'Pass multiple times to run multiple profilers. '
                           'Available profilers: %s' % profilers)
  option_parser.add_option('--tool',
                           dest='tool',
                           help='Run the test under a tool '
                           '(use --tool help to list them)')
  return option_parser
