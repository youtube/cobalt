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


import unittest

import trampoline  # pylint: disable=relative-import
import run_cobalt  # pylint: disable=relative-import
import run_tests  # pylint: disable=relative-import


class TrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions"""
  def setUp(self):
    # Change the trampoline internals for testing purposes.
    self.prev_platform, trampoline.PLATFORM = trampoline.PLATFORM, 'MY_PLATFORM'
    self.prev_config, trampoline.CONFIG = trampoline.CONFIG, 'MY_CONFIG'

  def tearDown(self):
    trampoline.PLATFORM = self.prev_platform
    trampoline.CONFIG = self.prev_config

  def testResolvePlatformConfig(self):
    """Tests that a device_id resolves to the expected value."""
    INPUT = [
      'python dummy.py', '{platform_arg}', '{config_arg}', '{device_id_arg}',
      '{target_params_arg}',
    ]
    EXPECTED_OUTPUT = (
        'python dummy.py --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT, argv=[])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)

  def testResolveDeviceId(self):
    """Tests that a device_id resolves to the expected value."""
    INPUT = [
      'python dummy.py', '{platform_arg}', '{config_arg}', '{device_id_arg}',
      '{target_params_arg}',
    ]
    EXPECTED_OUTPUT = (
      'python dummy.py --platform MY_PLATFORM --config MY_CONFIG'
      ' --device_id 1234')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT,
        argv=['--device_id', '1234'])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)

  def testTargetParams(self):
    """Tests that target_params resolves to the expected value."""
    INPUT = [
      'python dummy.py', '--target_name cobalt',
      '{platform_arg}', '{config_arg}', '{device_id_arg}',
      '{target_params_arg}',
    ]
    EXPECTED_OUTPUT = (
      'python dummy.py'
      ' --target_name cobalt --platform MY_PLATFORM'
      ' --config MY_CONFIG --target_params="--url=http://my.server.test"')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT,
        argv=['--target_params', '"--url=http://my.server.test"'])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)


class RunCobaltTrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions for run_cobalt.py"""
  def setUp(self):
    # Change the trampoline internals for testing purposes.
    self.prev_platform, trampoline.PLATFORM = trampoline.PLATFORM, 'MY_PLATFORM'
    self.prev_config, trampoline.CONFIG = trampoline.CONFIG, 'MY_CONFIG'

  def tearDown(self):
    trampoline.PLATFORM = self.prev_platform
    trampoline.CONFIG = self.prev_config

  def testGeneral(self):
    """Tests that target_params resolves to the expected value."""
    INPUT = run_cobalt.TRAMPOLINE
    EXPECTED_OUTPUT = (
      'python starboard/tools/example/app_launcher_client.py'
      ' --target_name cobalt --platform MY_PLATFORM --config MY_CONFIG'
      ' --target_params="--url=http://my.server.test"')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT,
        argv=['--target_params', '"--url=http://my.server.test"'])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)


class RunUnitTestsTrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions for run_unit_tests.py"""
  def setUp(self):
    # Change the trampoline internals for testing purposes.
    self.prev_platform, trampoline.PLATFORM = trampoline.PLATFORM, 'MY_PLATFORM'
    self.prev_config, trampoline.CONFIG = trampoline.CONFIG, 'MY_CONFIG'

  def tearDown(self):
    trampoline.PLATFORM = self.prev_platform
    trampoline.CONFIG = self.prev_config

  def testOne(self):
    """Tests that --target_name resolves to the expected value."""
    INPUT = run_tests.TRAMPOLINE
    EXPECTED_OUTPUT = (
      'python starboard/tools/testing/test_runner.py'
      ' --target_name nplb --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT,
        argv=['--target_name', 'nplb'])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)

  def testTwo(self):
    """Tests that --target_name used twice resolves to the expected value."""
    INPUT = run_tests.TRAMPOLINE
    EXPECTED_OUTPUT = (
      'python starboard/tools/testing/test_runner.py'
      ' --target_name nplb --target_name nb_test'
      ' --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str = trampoline.ResolveTrampoline(
        INPUT,
        argv=['--target_name', 'nplb', '--target_name', 'nb_test'])
    self.assertEqual(EXPECTED_OUTPUT, cmd_str)


if __name__ == '__main__':
  unittest.main(verbosity=2)
