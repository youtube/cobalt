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

import trampoline  # pylint: disable=relative-import,g-bad-import-order,g-import-not-at-top


class TrampolineTest(unittest.TestCase):
  """Tests trampoline substitutions."""

  def setUp(self):
    super(TrampolineTest, self).setUp()
    # Change the trampoline internals for testing purposes.
    trampoline.PLATFORM = 'MY_PLATFORM'
    trampoline.CONFIG ='MY_CONFIG'

  def testResolvePlatformConfig(self):
    """Tests that a device_id resolves to the expected value."""
    tramp = [
        'python dummy.py', '{platform_arg}', '{config_arg}', '{device_id_arg}',
        '{target_params_arg}',
    ]
    expected_output = (
        'python dummy.py --platform MY_PLATFORM --config MY_CONFIG')
    cmd_str, _ = trampoline.ResolveTrampoline(tramp, argv=[])
    self.assertEqual(expected_output, cmd_str)

  def testResolveDeviceId(self):
    """Tests that a device_id resolves to the expected value."""
    tramp = [
        'python dummy.py', '{platform_arg}', '{config_arg}', '{device_id_arg}',
        '{target_params_arg}',
    ]
    expected_output = (
        'python dummy.py --platform MY_PLATFORM --config MY_CONFIG'
        ' --device_id 1234')
    cmd_str, _ = trampoline.ResolveTrampoline(
        tramp,
        argv=['--device_id', '1234'])
    self.assertEqual(expected_output, cmd_str)

  def testTargetParams(self):
    """Tests that target_params resolves to the expected value."""
    tramp = [
        'python dummy.py', '--target_name cobalt',
        '{platform_arg}', '{config_arg}', '{device_id_arg}',
        '{target_params_arg}',
    ]
    expected_output = (
        'python dummy.py'
        ' --target_name cobalt --platform MY_PLATFORM'
        ' --config MY_CONFIG --target_params="--url=http://my.server.test"')
    cmd_str, _ = trampoline.ResolveTrampoline(
        tramp,
        argv=['--target_params', '"--url=http://my.server.test"'])
    self.assertEqual(expected_output, cmd_str)


if __name__ == '__main__':
  unittest.main(verbosity=2)
