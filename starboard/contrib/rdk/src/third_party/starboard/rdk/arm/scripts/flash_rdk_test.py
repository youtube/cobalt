#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.

"""Unit tests for the flash_rdk script."""

import os
from pathlib import Path
import sys
import unittest
from unittest import mock

# Ensure local script directory takes precedence in sys.path
_SCRIPT_DIR = str(Path(__file__).resolve().parent)
if _SCRIPT_DIR not in sys.path:
  sys.path.insert(0, _SCRIPT_DIR)
sys.modules.pop("flash_rdk", None)

import flash_rdk


class TestFlashRdk(unittest.TestCase):
  """Unit tests for flash_rdk.py."""

  def setUp(self):
    super().setUp()
    self.mock_sub_run = mock.patch.object(flash_rdk.subprocess, "run").start()
    self.mock_sub_popen = mock.patch.object(flash_rdk.subprocess, "Popen").start()

    self.mock_proc = mock.MagicMock()
    self.mock_proc.stdout.read.side_effect = [b"Burning %50\nComplete 100%\n", b""]
    self.mock_proc.poll.return_value = 0
    self.mock_proc.returncode = 0
    self.mock_sub_popen.return_value.__enter__.return_value = self.mock_proc

    mock.patch("os.path.exists", return_value=True).start()
    mock.patch("os.chmod").start()
    self.mock_exit = mock.patch("sys.exit", side_effect=SystemExit).start()

  def tearDown(self):
    mock.patch.stopall()
    super().tearDown()

  def test_check_and_reboot_adb_success(self):
    """Verifies ADB detection and reboot command invocation."""
    def sub_run_side_effect(cmd, *args, **kwargs):
      cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
      if "adb devices" in cmd_str:
        return mock.MagicMock(returncode=0, stdout="List of devices attached\nGZ21090420700209\tdevice\n")
      return mock.MagicMock(returncode=0, stdout="")

    self.mock_sub_run.side_effect = sub_run_side_effect
    flash_rdk.check_and_reboot_adb()

    reboot_calls = [
        call for call in self.mock_sub_run.call_args_list
        if "reboot" in str(call)
    ]
    self.assertTrue(len(reboot_calls) >= 1)

  def test_check_and_reboot_adb_no_device(self):
    """Verifies that check_and_reboot_adb exits if no ADB device is attached."""
    self.mock_sub_run.return_value = mock.MagicMock(returncode=0, stdout="List of devices attached\n\n")
    with self.assertRaises(SystemExit):
      flash_rdk.check_and_reboot_adb()

  def test_flash_image_success(self):
    """Verifies execution of the flashing tool and successful completion."""
    with mock.patch.object(flash_rdk, "open", mock.mock_open(), create=True):
      flash_rdk.flash_image("/path/to/tool", "/path/to/image.img", erase=1, reboot=1)

    self.mock_sub_popen.assert_called_once()
    self.mock_proc.wait.assert_called_once()


if __name__ == "__main__":
  unittest.main()
