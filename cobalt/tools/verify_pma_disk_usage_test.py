#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Tests for verify_pma_disk_usage.py."""

import shlex
import unittest
from unittest.mock import MagicMock, patch

import verify_pma_disk_usage as pma


class VerifyPmaDiskUsageTest(unittest.TestCase):
  """Tests for verify_pma_disk_usage.py."""

  @patch("verify_pma_disk_usage.subprocess.run")
  def test_app_commands_run_inside_run_as(self, mock_run):
    cmd = "stat -c '%s %n' /data/data/pkg/BrowserStabilityMetrics/*.pma"
    mock_run.return_value = MagicMock(returncode=1, stdout="", stderr="")
    pma.run_adb_shell(cmd, "SERIAL", package="pkg")
    # The whole command, including the glob, runs inside run-as.
    self.assertEqual(mock_run.call_args.args[0], [
        "adb", "-s", "SERIAL", "shell", f"run-as pkg sh -c {shlex.quote(cmd)}"
    ])
    # The plain shell is used only when run-as cannot be used.
    mock_run.side_effect = [
        MagicMock(
            returncode=1, stdout="", stderr="run-as: unknown package: pkg"),
        MagicMock(returncode=0, stdout="131072 a.pma", stderr="")
    ]
    self.assertEqual(
        pma.run_adb_shell(cmd, "SERIAL", package="pkg"), (0, "131072 a.pma"))
    self.assertEqual(mock_run.call_args.args[0],
                     ["adb", "-s", "SERIAL", "shell", cmd])

  @patch("verify_pma_disk_usage.time")
  @patch("verify_pma_disk_usage.list_pma_files")
  def test_poll_fails_closed(self, mock_list, mock_time):
    mock_list.return_value = (None, None, "No BrowserStabilityMetrics")
    mock_time.time.side_effect = [0.0, 0.0, 5.0]
    self.assertEqual(
        pma.poll_pma_usage(None, "pkg", 1.0, 1.0), pma.EXIT_CANNOT_MEASURE)

  @patch("verify_pma_disk_usage.time", new=MagicMock(monotonic=lambda: 0.0))
  @patch(
      "verify_pma_disk_usage.get_dir_total_disk_usage_kb",
      new=MagicMock(return_value=None))
  @patch("verify_pma_disk_usage.list_pma_files")
  @patch("verify_pma_disk_usage.run_adb_shell")
  def test_stress_kills_when_new_file_appears(self, mock_shell, mock_list):
    mock_shell.return_value = (0, "Starting: Intent")
    old = {"name": "old.pma", "size_bytes": 131072, "size_kb": 128.0}
    new = {"name": "new.pma", "size_bytes": 131072, "size_kb": 128.0}
    mock_list.side_effect = [(131072, [old], "dir"), (262144, [old,
                                                               new], "dir"),
                             (262144, [old, new], "dir")]
    self.assertEqual(
        pma.run_stress_launches(None, "com.example.tv", 1), pma.EXIT_PASS)
    commands = [c.args[0] for c in mock_shell.call_args_list]
    self.assertEqual(commands[1:], [
        "am start -a android.intent.action.VIEW -d "
        "https://www.youtube.com/tv -p com.example.tv",
        "am force-stop com.example.tv"
    ])


if __name__ == "__main__":
  unittest.main()
