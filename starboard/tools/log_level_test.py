#!/usr/bin/env python
#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Tests the log_level module."""

import argparse
import logging
import mock
import unittest
import _env  # pylint: disable=unused-import
from starboard.tools import log_level

_INITIALIZE_LOGGING_MOCK = 'starboard.tools.log_level.InitializeLoggingWithLevel'


class LogLevelTest(unittest.TestCase):

  @mock.patch(_INITIALIZE_LOGGING_MOCK)
  def testRainyDayNoArgs(self, initialize_logging_mock):
    log_level.InitializeLogging(None)
    initialize_logging_mock.assert_called_with(logging.INFO)

  @mock.patch(_INITIALIZE_LOGGING_MOCK)
  def testRainyDayEmptyArgs(self, initialize_logging_mock):
    log_level.InitializeLogging(argparse.Namespace())
    initialize_logging_mock.assert_called_with(logging.INFO)

  @mock.patch(_INITIALIZE_LOGGING_MOCK)
  def testSunnyDayCorrectLevels(self, initialize_logging_mock):
    for name, level in log_level._NAME_TO_LEVEL.items():
      args = argparse.Namespace()
      args.log_level = name
      log_level.InitializeLogging(args)
      initialize_logging_mock.assert_called_with(level)

  @mock.patch(_INITIALIZE_LOGGING_MOCK)
  def testSunnyDayVerboseOverride(self, initialize_logging_mock):
    args = argparse.Namespace()
    args.verbose = True
    log_level.InitializeLogging(args)
    initialize_logging_mock.assert_called_with(logging.DEBUG)


if __name__ == '__main__':
  unittest.main()
