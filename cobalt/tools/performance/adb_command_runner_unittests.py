#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""ADB command runner tests the various expected outputs from
   the command runner module.
"""

import unittest

from adb_command_runner import run_adb_command


# Mock helper methods to mock out subprocess library commands
def mock_success_runner(cmd, shell, timeout):
  del cmd, shell, timeout  # unused
  return "Mock stdout", "", 0


def mock_fail_runner(cmd, shell, timeout):
  del cmd, shell, timeout  # unused
  return "", "Mock stderr", 1


def mock_timeout_runner(cmd, shell, timeout):
  del cmd, shell, timeout  # unused
  return "", "Command timed out", -1


class PerfTest(unittest.TestCase):

  def testCommandSuccess(self):
    stdout, stderr_msg = run_adb_command(
        "adb devices", subprocess_runner=mock_success_runner)
    self.assertEqual(stdout, "Mock stdout")
    self.assertEqual(stderr_msg, None)

  def testCommandFailure(self):
    stdout, stderr_msg = run_adb_command(
        "adb shell some_bad_command", subprocess_runner=mock_fail_runner)
    self.assertEqual(stdout, "")
    self.assertTrue("Mock stderr" in stderr_msg)

  def testCommandTimeout(self):
    stdout, stderr_msg = run_adb_command(
        "adb long_running_command",
        timeout=1,
        subprocess_runner=mock_timeout_runner)
    self.assertEqual(stdout, None)
    self.assertEqual(stderr_msg, "Command timed out")
