#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Tests for Raspi launcher"""

import logging
from starboard.raspi.shared import launcher
import sys
import argparse
import unittest
import os
from unittest.mock import patch, ANY, call, Mock
import tempfile
from pathlib import Path
import pexpect

# pylint: disable=missing-class-docstring


@unittest.skipIf(os.name == 'nt', 'Pexpect does not work on Windows')
class LauncherTest(unittest.TestCase):

  def setUp(self):
    self.target = 'baz'
    self.device_id = '198.51.100.1'  # Reserved address
    # Current launcher requires real files, so we generate one
    # pylint: disable=consider-using-with
    self.tmpdir = tempfile.TemporaryDirectory()
    target_path = os.path.join(self.tmpdir.name, 'install', self.target)
    os.makedirs(target_path)
    Path(os.path.join(target_path, self.target)).touch()
    # Minimal set of params required to crete one
    self.params = {
        'device_id': self.device_id,
        'platform': 'raspi-2',
        'target_name': self.target,
        'config': 'test',
        'out_directory': self.tmpdir.name
    }
    self.fake_timeout = 0.11

  # pylint: disable=protected-access
  def _make_launcher(self):
    launcher.Launcher._PEXPECT_TIMEOUT = self.fake_timeout
    launcher.Launcher._PEXPECT_PASSWORD_TIMEOUT_MAX_RETRIES = 0
    launcher.Launcher._PEXPECT_SHUTDOWN_SLEEP_TIME = 0.12
    launcher.Launcher._INTER_COMMAND_DELAY_SECONDS = 0.013
    launcher.Launcher._PEXPECT_READLINE_TIMEOUT_MAX_RETRIES = 2
    launch = launcher.Launcher(**self.params)
    return launch


class LauncherAPITest(LauncherTest):

  def test_construct(self):
    launch = self._make_launcher()
    self.assertIsNotNone(launch)
    self.assertEqual(launch.device_id, self.device_id)
    self.assertEqual(launch.platform_name, 'raspi-2')
    self.assertEqual(launch.target_name, self.target)
    self.assertEqual(launch.config, 'test')
    self.assertEqual(launch.out_directory, self.tmpdir.name)

  def test_run(self):
    result = self._make_launcher().Run()
    # Expect test failure
    self.assertEqual(result, 1)

  def test_ip(self):
    self.assertEqual(self._make_launcher().GetDeviceIp(), self.device_id)

  def test_output(self):
    # The path is hardcoded in the launcher
    self.assertEqual(self._make_launcher().GetDeviceOutputPath(), '/tmp')

  def test_kill(self):
    self.assertIsNone(self._make_launcher().Kill())


class StringContains(str):

  def __eq__(self, value):
    return self in value


# Tests here test implementation details, rather than behavior.
# pylint: disable=protected-access
class LauncherInternalsTest(LauncherTest):

  def setUp(self):
    super().setUp()
    self.launch = self._make_launcher()
    self.launch.pexpect_process = Mock(
        spec_set=['expect', 'sendline', 'readline'])

  @patch('starboard.raspi.shared.launcher.pexpect.spawn')
  def test_spawn(self, spawn):
    mock_pexpect = spawn.return_value
    self.launch._PexpectSpawnAndConnect('echo test')
    spawn.assert_called_once_with('echo test', timeout=ANY, encoding=ANY)
    mock_pexpect.sendline.assert_called_once_with(
        'echo cobalt-launcher-login-success')
    mock_pexpect.expect.assert_any_call(['cobalt-launcher-login-success'])

  def test_sleep(self):
    self.launch._Sleep(42)
    self.launch.pexpect_process.sendline.assert_called_once_with(
        'sleep 42;echo cobalt-launcher-done-sleeping')
    self.launch.pexpect_process.expect.assert_called_once_with(
        ['cobalt-launcher-done-sleeping'])

  def test_waitforconnect(self):
    self.launch._WaitForPrompt()
    self.launch.pexpect_process.expect.assert_called_once_with(
        'pi@raspberrypi:')

    # trigger one timeout
    self.launch.pexpect_process.expect = Mock(
        side_effect=[pexpect.TIMEOUT(1), None])
    self.launch._WaitForPrompt()
    self.launch.pexpect_process.expect.assert_has_calls([
        call('pi@raspberrypi:'),
        call('pi@raspberrypi:'),
    ])

    # infinite timeout
    self.launch.pexpect_process.expect = Mock(side_effect=pexpect.TIMEOUT(1))
    with self.assertRaises(pexpect.TIMEOUT):
      self.launch._WaitForPrompt()

  def test_readlines(self):
    # Return empty string
    self.launch.pexpect_process.readline = Mock(return_value='')
    self.launch._PexpectReadLines()
    self.launch.pexpect_process.readline.assert_called_once()
    self.assertIsNone(getattr(self.launch, 'return_value', None))

    # Return default success tag
    self.launch.pexpect_process.readline = Mock(
        return_value=self.launch.test_complete_tag)
    self.launch._PexpectReadLines()
    self.launch.pexpect_process.readline.assert_called_once()
    # This is a bug
    self.assertIsNone(getattr(self.launch, 'return_value', None))

    line = self.launch.test_complete_tag + self.launch.test_success_tag
    self.launch.pexpect_process.readline = Mock(return_value=line)
    self.launch._PexpectReadLines()
    self.assertEqual(self.launch.return_value, 0)

    self.launch.pexpect_process.readline = Mock(side_effect=pexpect.TIMEOUT(1))
    with self.assertRaises(pexpect.TIMEOUT):
      self.launch._PexpectReadLines()

  def test_readlines_multiple(self):
    self.launch.pexpect_process.readline = Mock(side_effect=['abc', 'bbc', ''])
    self.launch._PexpectReadLines()
    self.assertEqual(3, self.launch.pexpect_process.readline.call_count)

    self.launch.pexpect_process.readline = Mock(
        side_effect=['abc', 'bbc', '', 'none'])
    self.launch._PexpectReadLines()
    self.assertEqual(3, self.launch.pexpect_process.readline.call_count)

  def test_kill_processes(self):
    self.launch._KillExistingCobaltProcesses()
    self.launch.pexpect_process.sendline.assert_any_call(
        StringContains('pkill'))

  @patch('starboard.raspi.shared.launcher.pexpect.spawn')
  def test_run_with_mock(self, spawn):
    pexpect_ = Mock()
    pexpect_.readline = Mock(return_value='')
    spawn.return_value = pexpect_
    self.launch.Run()
    self.assertEqual(self.launch.return_value, 1)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('device_id')
  parser.add_argument('--target', default='eztime_test')
  parser.add_argument('--out_directory')
  parser.add_argument('--config', default='devel')
  parser.add_argument('--verbose', '-v', action='store_true')
  args = parser.parse_args()
  logging.basicConfig(
      stream=sys.stdout, level=logging.DEBUG if args.verbose else logging.INFO)
  path = os.path.join(
      os.path.dirname(launcher.__file__), f'../../../out/raspi-2_{args.config}')
  logging.info('path: %s', path)
  launch_test = launcher.Launcher(
      platform='raspi-2',
      target_name=args.target,
      config=args.config,
      device_id=args.device_id,
      out_directory=path)
  launch_test.Run()
