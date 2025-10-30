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

import unittest
from unittest.mock import MagicMock, call

from smaps_capture import SmapsCapturer, main
from unittest.mock import patch, MagicMock, call


class SmapsCaptureTest(unittest.TestCase):
    """Tests for smaps_capture.py."""

    def setUp(self):
        """Set up common mocks for tests."""
        self.mock_subprocess = MagicMock()
        self.mock_time = MagicMock()
        self.mock_os = MagicMock()
        self.mock_datetime = MagicMock()
        self.mock_open = MagicMock()

        # Configure the mock for datetime.datetime.now()
        self.mock_now = MagicMock()
        self.mock_now.strftime.return_value = "20250101_120000"
        self.mock_datetime.datetime.now.return_value = self.mock_now

    def create_capturer(self, **kwargs):
        """Creates an instance of SmapsCapturer with mocked dependencies."""
        args = {
            'process_name': "test.process",
            'device_serial': "test-serial",
            'adb_path': "adb",
            'interval_minutes': 1,
            'capture_duration_seconds': 120,
            'output_dir': "test_logs",
            'subprocess_module': self.mock_subprocess,
            'time_module': self.mock_time,
            'os_module': self.mock_os,
            'datetime_module': self.mock_datetime,
        }
        args.update(kwargs)
        capturer = SmapsCapturer(**args)
        capturer.open = self.mock_open
        return capturer

    @patch('smaps_capture.SmapsCapturer')
    @patch('argparse.ArgumentParser')
    def test_main_argument_parsing(self, mock_argparse, mock_capturer):
        """Tests that command-line arguments are correctly parsed and passed."""
        mock_args = MagicMock()
        mock_args.process_name = "custom.process"
        mock_args.interval_minutes = 5
        mock_args.capture_duration_seconds = 300
        mock_args.output_dir = "custom_logs"
        mock_args.device_serial = "custom-serial"
        mock_args.adb_path = "/custom/adb"
        mock_argparse.return_value.parse_args.return_value = mock_args

        main()

        mock_capturer.assert_called_once_with(
            process_name="custom.process",
            interval_minutes=5,
            capture_duration_seconds=300,
            output_dir="custom_logs",
            device_serial="custom-serial",
            adb_path="/custom/adb")
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

    def test_get_pid_success(self):
        """Tests successful PID retrieval."""
        mock_process = MagicMock()
        mock_process.returncode = 0
        mock_process.stdout = '12345\r\n'
        self.mock_subprocess.run.return_value = mock_process

        capturer = self.create_capturer()
        pid = capturer.get_pid()

        self.assertEqual(pid, '12345')
        self.mock_subprocess.run.assert_called_once()

    def test_get_pid_not_found(self):
        """Tests when the process is not found."""
        mock_process = MagicMock()
        mock_process.returncode = 1
        mock_process.stdout = ''
        self.mock_subprocess.run.return_value = mock_process

        capturer = self.create_capturer()
        pid = capturer.get_pid()

        self.assertIsNone(pid)

    def test_get_pid_exception(self):
        """Tests exception handling during PID retrieval."""
        self.mock_subprocess.run.side_effect = Exception("ADB error")
        capturer = self.create_capturer()
        pid = capturer.get_pid()
        self.assertIsNone(pid)

    def test_capture_smaps_success(self):
        """Tests a successful smaps capture."""
        mock_process = MagicMock()
        mock_process.returncode = 0
        mock_process.stdout = "smaps content"
        self.mock_subprocess.run.return_value = mock_process
        self.mock_os.path.join.return_value = "test_logs/smaps_20250101_120000_12345.txt"
        self.mock_os.path.getsize.return_value = 1024

        capturer = self.create_capturer()
        capturer.capture_smaps('12345')

        self.mock_open.assert_called_with(
            "test_logs/smaps_20250101_120000_12345.txt", 'w')
        self.mock_open().__enter__().write.assert_called_with("smaps content")
        self.mock_os.path.getsize.assert_called()
        self.mock_os.remove.assert_not_called()

    def test_capture_smaps_failure(self):
        """Tests a failed smaps capture."""
        mock_process = MagicMock()
        mock_process.returncode = 1
        mock_process.stderr = "error"
        self.mock_subprocess.run.return_value = mock_process
        self.mock_os.path.join.return_value = "test_logs/smaps_20250101_120000_12345.txt"
        self.mock_os.path.exists.return_value = True

        capturer = self.create_capturer()
        capturer.capture_smaps('1234f')

        self.mock_os.remove.assert_called_with(
            "test_logs/smaps_20250101_120000_12345.txt")

    def test_run_capture_cycles(self):
        """Tests the main capture loop."""
        capturer = self.create_capturer(
            capture_duration_seconds=120, interval_minutes=1)
        capturer.get_pid = MagicMock(side_effect=['12345', '54321', None])
        capturer.capture_smaps = MagicMock()

        # Simulate time passing
        self.mock_time.time.side_effect = [0, 1, 61, 121]

        capturer.run_capture_cycles()

        self.mock_os.makedirs.assert_called_with("test_logs", exist_ok=True)
        # Assert that the loop ran the expected number of times and then exited.
        # For capture_duration_seconds=120 and interval_seconds=60, this means 2 iterations.
        self.assertEqual(capturer.get_pid.call_count, 2)
        self.assertEqual(capturer.capture_smaps.call_count, 2)
        capturer.capture_smaps.assert_has_calls(
            [call('12345'), call('54321')])
        self.mock_time.sleep.assert_has_calls([call(60), call(60)])

    def test_run_capture_cycles_different_duration(self):
        """Tests the main capture loop with a different duration and interval."""
        capturer = self.create_capturer(
            capture_duration_seconds=180, interval_minutes=1)
        capturer.get_pid = MagicMock(side_effect=['111', '222', '333', None])
        capturer.capture_smaps = MagicMock()

        # Simulate time passing for 3 iterations (180s duration / 60s interval)
        self.mock_time.time.side_effect = [0, 1, 61, 121, 181]

        capturer.run_capture_cycles()

        self.mock_os.makedirs.assert_called_with("test_logs", exist_ok=True)
        # For capture_duration_seconds=180 and interval_seconds=60, this means 3 iterations.
        self.assertEqual(capturer.get_pid.call_count, 3)
        self.assertEqual(capturer.capture_smaps.call_count, 3)
        capturer.capture_smaps.assert_has_calls(
            [call('111'), call('222'), call('333')])
        self.mock_time.sleep.assert_has_calls([call(60), call(60), call(60)])


if __name__ == '__main__':
    unittest.main()
