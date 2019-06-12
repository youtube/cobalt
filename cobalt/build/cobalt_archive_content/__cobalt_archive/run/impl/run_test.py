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
import run  # pylint: disable=relative-import,g-bad-import-order,g-import-not-at-top


class RunGeneralTrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions for run_cobalt.py."""

  def setUp(self):
    """Change the trampoline internals for testing purposes."""
    super(RunGeneralTrampolineTest, self).setUp()
    run.trampoline.PLATFORM = 'MY_PLATFORM'
    run.trampoline.CONFIG = 'MY_CONFIG'

  def testOneTarget(self):
    """Tests that one target gets resolved."""
    expected_output = (
        'python starboard/tools/example/app_launcher_client.py'
        ' --platform MY_PLATFORM --config MY_CONFIG'
        ' --target_name cobalt')
    cmd_str = run._ResolveTrampoline(argv=['cobalt'])
    self.assertEqual(expected_output, cmd_str)

  def testTwoTargets(self):
    """Tests that two targets gets resolved."""
    expected_output = (
        'python starboard/tools/example/app_launcher_client.py'
        ' --platform MY_PLATFORM --config MY_CONFIG'
        ' --target_name cobalt --target_name nplb')
    cmd_str = run._ResolveTrampoline(argv=['cobalt', 'nplb'])
    self.assertEqual(expected_output, cmd_str)

  def testTargetParams(self):
    """Tests that the the target_params gets correctly resolved."""
    expected_output = (
        'python starboard/tools/example/app_launcher_client.py'
        ' --platform MY_PLATFORM --config MY_CONFIG'
        ' --target_params="--url=http://my.server.test"')
    argv = ['--target_params', '"--url=http://my.server.test"']
    cmd_str = run._ResolveTrampoline(argv=argv)
    self.assertEqual(expected_output, cmd_str)


if __name__ == '__main__':
  unittest.main(verbosity=2)
