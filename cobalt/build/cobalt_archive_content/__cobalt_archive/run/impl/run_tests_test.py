#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


"""Unit tests the run.py trampoline."""

import sys
import unittest

import trampoline  # pylint: disable=relative-import,g-bad-import-order,g-import-not-at-top
sys.path.insert(0, '..')
import run_tests  # pylint: disable=relative-import,g-bad-import-order,g-import-not-at-top


class RunUnitTestsTrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions for run_unit_tests.py."""

  def setUp(self):
    """Change the trampoline internals for testing purposes."""
    super(RunUnitTestsTrampolineTest, self).setUp()
    self.prev_platform, trampoline.PLATFORM = trampoline.PLATFORM, 'MY_PLATFORM'
    self.prev_config, trampoline.CONFIG = trampoline.CONFIG, 'MY_CONFIG'

  def tearDown(self):
    super(RunUnitTestsTrampolineTest, self).tearDown()
    trampoline.PLATFORM = self.prev_platform
    trampoline.CONFIG = self.prev_config

  def testOne(self):
    """Tests that --target_name resolves to the expected value."""
    tramp = run_tests.TRAMPOLINE
    expected_output = (
        'python starboard/tools/testing/test_runner.py'
        ' --target_name nplb --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = trampoline.ResolveTrampoline(
        tramp,
        argv=['--target_name', 'nplb'])
    self.assertEqual(expected_output, cmd_str)

  def testTwo(self):
    """Tests that --target_name used twice resolves to the expected value."""
    tramp = run_tests.TRAMPOLINE
    expected_output = (
        'python starboard/tools/testing/test_runner.py'
        ' --target_name nplb --target_name nb_test'
        ' --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = trampoline.ResolveTrampoline(
        tramp,
        argv=['--target_name', 'nplb', '--target_name', 'nb_test'])
    self.assertEqual(expected_output, cmd_str)


if __name__ == '__main__':
  unittest.main(verbosity=2)
