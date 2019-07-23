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

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
import run_tests  # pylint: disable=relative-import,g-bad-import-order,g-import-not-at-top


class RunUnitTestsTrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions for run_unit_tests.py."""

  def setUp(self):
    """Change the trampoline internals for testing purposes."""
    super(RunUnitTestsTrampolineTest, self).setUp()
    run_tests.trampoline.PLATFORM = 'MY_PLATFORM'
    run_tests.trampoline.CONFIG = 'MY_CONFIG'

  def testOne(self):
    """Tests that --target_name resolves to the expected value."""
    expected_output = (
        'python starboard/tools/testing/test_runner.py'
        ' --target_name nplb --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = run_tests._ResolveTrampoline(argv=['--target_name', 'nplb'])
    self.assertEqual(expected_output, cmd_str)

  def testTwo(self):
    """Tests that --target_name used twice resolves to the expected value."""
    expected_output = (
        'python starboard/tools/testing/test_runner.py'
        ' --platform MY_PLATFORM --config MY_CONFIG'
        ' --target_name nplb --target_name nb_test')
    argv=['nplb', 'nb_test']
    cmd_str = run_tests._ResolveTrampoline(argv=argv)
    self.assertEqual(expected_output, cmd_str)


if __name__ == '__main__':
  unittest.main(verbosity=2)
