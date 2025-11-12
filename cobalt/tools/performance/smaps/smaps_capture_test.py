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
"""Tests for smaps_capture.py."""

import unittest
from unittest.mock import call, MagicMock, patch

from smaps_capture import SmapsCapturer, run_smaps_capture_tool


class SmapsCaptureTest(unittest.TestCase):
  """Tests for smaps_capture.py."""

  def setUp(self):
    """Set up common mocks for tests."""
    self.mock_subprocess = MagicMock()
    self.mock_time = MagicMock()
    self.mock_os = MagicMock()
    self.mock_datetime = MagicMock()
    self.mock_open = MagicMock()
    self.mock_file_handle = MagicMock()
    self.mock_open.return_value.__enter__.return_value = self.mock_file_handle

    # Configure the mock for datetime.datetime.now()
    self.mock_now = MagicMock()
    self.mock_now.strftime.return_value = '20250101_120000'
    self.mock_datetime.datetime.now.return_value = self.mock_now

  def create_capturer(self, **kwargs):
    """Creates an instance of SmapsCapturer with mocked dependencies."""
    config = MagicMock()
    config.process_name = 'test.process'
    config.platform = 'android'  # Default to android for existing tests
    config.device_serial = 'test-serial'
    config.adb_path = 'adb'
    config.interval_minutes = 1
    config.capture_duration_seconds = 120
    config.output_dir = 'test_logs'

    for key, value in kwargs.items():
      setattr(config, key, value)

    capturer = SmapsCapturer(
        config,
        subprocess_module=self.mock_subprocess,
        time_module=self.mock_time,
        os_module=self.mock_os,
        datetime_module=self.mock_datetime)
    capturer.open = self.mock_open
    return capturer

  @patch('smaps_capture.SmapsCapturer')
  def test_main_argument_parsing(self, mock_capturer):
    """Tests that command-line arguments are correctly parsed and passed."""
    # Simulate command-line arguments
    test_argv = [
        '--platform', 'linux', '--process_name', 'custom.process',
        '--interval_minutes', '5', '--capture_duration_seconds', '300',
        '--output_dir', 'custom_logs', '--device_serial', 'custom-serial',
        '--adb_path', '/custom/adb'
    ]

    run_smaps_capture_tool(test_argv)

    # Check that the capturer was called with an object that has the correct
    # attributes
    mock_capturer.assert_called_once()
    args, _ = mock_capturer.call_args
    self.assertEqual(args[0].platform, 'linux')
    self.assertEqual(args[0].process_name, 'custom.process')
    self.assertEqual(args[0].interval_minutes, 5)
    self.assertEqual(args[0].capture_duration_seconds, 300)
    self.assertEqual(args[0].output_dir, 'custom_logs')
    self.assertEqual(args[0].device_serial, 'custom-serial')
    self.assertEqual(args[0].adb_path, '/custom/adb')
    mock_capturer.return_value.run_capture_cycles.assert_called_once()

  def test_build_adb_command_with_serial(self):
    """Tests that the ADB command is correctly built with a device serial."""
    capturer = self.create_capturer(device_serial='test-serial')
    command = capturer.build_adb_command('shell', 'ls')
    self.assertEqual(command, ['adb', '-s', 'test-serial', 'shell', 'ls'])

  def test_build_adb_command_without_serial(self):
    """Tests that the ADB command is correctly built without a device serial."""
    capturer = self.create_capturer(device_serial=None)
    command = capturer.build_adb_command('shell', 'ls')
    self.assertEqual(command, ['adb', 'shell', 'ls'])

  def test_get_pids_android_success(self):
    """Tests successful PID retrieval on Android."""
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout = '12345\r\n'
    self.mock_subprocess.run.return_value = mock_process

    capturer = self.create_capturer(platform='android')
    pids = capturer.get_pids()

    self.assertEqual(pids, ['12345'])

  def test_get_pids_linux_success(self):
    """Tests successful PID retrieval on Linux."""
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout = '54321'
    self.mock_subprocess.run.return_value = mock_process

    capturer = self.create_capturer(platform='linux')
    pids = capturer.get_pids()

    self.assertEqual(pids, ['54321'])

  def test_get_pids_linux_multiple_pids(self):
    """Tests that the main PID is selected from multiple PIDs."""
    # Mock the pidof command to return multiple PIDs
    mock_pidof_process = MagicMock()
    mock_pidof_process.returncode = 0
    mock_pidof_process.stdout = '12345 54321 67890'

    # Mock the ps command to return different elapsed times
    mock_ps_process = MagicMock()
    mock_ps_process.returncode = 0
    mock_ps_process.stdout = '  PID  ELAPSED\n12345      ' \
            + '100\n54321      500\n67890      200'

    self.mock_subprocess.run.side_effect = [mock_pidof_process, mock_ps_process]

    capturer = self.create_capturer(platform='linux')
    pids = capturer.get_pids()

    self.assertEqual(pids, ['54321'])
    self.mock_subprocess.run.assert_has_calls([
        call(['pidof', 'test.process'],
             capture_output=True,
             text=True,
             check=False,
             timeout=10),
        call(['ps', '-o', 'pid,etimes', '-p', '12345,54321,67890'],
             capture_output=True,
             text=True,
             check=False,
             timeout=10)
    ])

  def test_get_pids_linux_empty_pidof_output(self):
    """Tests that an empty stdout from pidof is handled correctly."""
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout = '   '  # Whitespace only
    self.mock_subprocess.run.return_value = mock_process

    capturer = self.create_capturer(platform='linux')
    pids = capturer.get_pids()

    self.assertEqual(pids, [])

  def test_get_pids_dispatches_correctly(self):
    """Tests that get_pids calls the correct platform-specific method."""
    capturer_android = self.create_capturer(platform='android')
    capturer_android.get_pids_android = MagicMock(return_value=['android-pid'])
    self.assertEqual(capturer_android.get_pids(), ['android-pid'])
    capturer_android.get_pids_android.assert_called_once()

    capturer_linux = self.create_capturer(platform='linux')
    capturer_linux.get_pids_linux = MagicMock(return_value=['linux-pid'])
    self.assertEqual(capturer_linux.get_pids(), ['linux-pid'])
    capturer_linux.get_pids_linux.assert_called_once()

  def test_get_pids_not_found(self):
    """Tests when the process is not found."""
    mock_process = MagicMock()
    mock_process.returncode = 1
    mock_process.stdout = ''
    self.mock_subprocess.run.return_value = mock_process

    capturer = self.create_capturer()
    pids = capturer.get_pids()

    self.assertEqual(pids, [])

  def test_get_pids_exception(self):
    """Tests exception handling during PID retrieval."""
    self.mock_subprocess.run.side_effect = Exception('Error')
    capturer = self.create_capturer()
    pids = capturer.get_pids()
    self.assertEqual(pids, [])

  def test_capture_smaps_android_success(self):
    """Tests a successful smaps capture on Android."""
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout = 'android smaps content'
    self.mock_subprocess.run.return_value = mock_process
    self.mock_os.path.join.return_value = (
        'test_logs/smaps_20250101_120000_12345.txt')
    self.mock_os.path.getsize.return_value = 1024

    capturer = self.create_capturer(platform='android')
    capturer.capture_smaps('12345')

    self.mock_open.assert_called_with(
        'test_logs/smaps_20250101_120000_12345.txt', 'w', encoding='utf-8')
    self.mock_file_handle.write.assert_called_with('android smaps content')
    self.mock_os.path.getsize.assert_called()
    self.mock_os.remove.assert_not_called()

  def test_capture_smaps_linux_success(self):
    """Tests a successful smaps capture on Linux."""
    self.mock_os.path.join.return_value = (
        'test_logs/smaps_20250101_120000_54321.txt')
    self.mock_os.path.getsize.return_value = 1024

    smaps_content_lines = ['line1\n', 'line2\n', 'line3\n']

    # Create a mock for the input file (smaps_file)
    mock_smaps_file_read = MagicMock()
    mock_smaps_file_read.__enter__.return_value = mock_smaps_file_read
    mock_smaps_file_read.__iter__.return_value = iter(smaps_content_lines)

    # Create a mock for the output file (output_file)
    mock_output_file_write = MagicMock()
    mock_output_file_write.__enter__.return_value = mock_output_file_write

    # Configure self.mock_open to return these two mocks sequentially
    self.mock_open.side_effect = [
        mock_smaps_file_read,  # First call to open (for smaps_path)
        mock_output_file_write  # Second call to open (for output_filename)
    ]

    capturer = self.create_capturer(platform='linux')
    capturer.capture_smaps('54321')

    # Assert that open was called for both files
    self.mock_open.assert_has_calls([
        call('/proc/54321/smaps', 'r', encoding='utf-8'),
        call(
            'test_logs/smaps_20250101_120000_54321.txt', 'w', encoding='utf-8')
    ])

    # Assert that each line was written to the output file
    mock_output_file_write.write.assert_has_calls(
        [call(line) for line in smaps_content_lines])
    self.mock_os.path.getsize.assert_called_with(
        'test_logs/smaps_20250101_120000_54321.txt')

  def test_capture_smaps_dispatches_correctly(self):
    """Tests that capture_smaps calls the correct platform-specific method."""
    capturer_android = self.create_capturer(platform='android')
    capturer_android.capture_smaps_android = MagicMock()
    capturer_android.capture_smaps('android-pid')
    capturer_android.capture_smaps_android.assert_called_once_with(
        'android-pid')

    capturer_linux = self.create_capturer(platform='linux')
    capturer_linux.capture_smaps_linux = MagicMock()
    capturer_linux.capture_smaps('linux-pid')
    capturer_linux.capture_smaps_linux.assert_called_once_with('linux-pid')

  def test_capture_smaps_failure(self):
    """Tests a failed smaps capture."""
    mock_process = MagicMock()
    mock_process.returncode = 1
    mock_process.stderr = 'error'
    self.mock_subprocess.run.return_value = mock_process
    self.mock_os.path.join.return_value = (
        'test_logs/smaps_20250101_120000_12345.txt')
    self.mock_os.path.exists.return_value = True

    capturer = self.create_capturer(platform='android')
    capturer.capture_smaps('1234f')

    self.mock_os.remove.assert_called_with(
        'test_logs/smaps_20250101_120000_12345.txt')

  def test_run_capture_cycles(self):
    """Tests the main capture loop."""
    capturer = self.create_capturer(
        capture_duration_seconds=120, interval_minutes=1)
    capturer.get_pids = MagicMock(side_effect=[['12345'], ['54321'], []])
    capturer.capture_smaps = MagicMock()

    # Simulate time passing
    self.mock_time.time.side_effect = [0, 1, 61, 121]

    capturer.run_capture_cycles()

    self.mock_os.makedirs.assert_called_with('test_logs', exist_ok=True)
    # Assert that the loop ran the expected number of times and then exited.
    # For capture_duration_seconds=120 and interval_seconds=60, this means 2
    # iterations.
    self.assertEqual(capturer.get_pids.call_count, 2)
    self.assertEqual(capturer.capture_smaps.call_count, 2)
    capturer.capture_smaps.assert_has_calls([call('12345'), call('54321')])
    self.mock_time.sleep.assert_has_calls([call(60), call(60)])

  def test_run_capture_cycles_different_duration(self):
    """Tests the main capture loop with a different duration and interval."""
    capturer = self.create_capturer(
        capture_duration_seconds=180, interval_minutes=1)
    capturer.get_pids = MagicMock(side_effect=[['111'], ['222'], ['444'], []])
    capturer.capture_smaps = MagicMock()

    # Simulate time passing for 3 iterations (180s duration / 60s interval)
    self.mock_time.time.side_effect = [0, 1, 61, 121, 181]

    capturer.run_capture_cycles()

    self.mock_os.makedirs.assert_called_with('test_logs', exist_ok=True)
    # For capture_duration_seconds=180 and interval_seconds=60, this means 3
    # iterations.
    self.assertEqual(capturer.get_pids.call_count, 3)
    self.assertEqual(capturer.capture_smaps.call_count, 3)
    capturer.capture_smaps.assert_has_calls(
        [call('111'), call('222'), call('444')])
    self.mock_time.sleep.assert_has_calls([call(60), call(60), call(60)])


if __name__ == '__main__':
  unittest.main()
