# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Unit tests for verify_memory_accounting_coverage.py."""

# pylint: disable=inconsistent-quotes

import io
import os
import sys
import unittest
from unittest.mock import patch

# Add current directory to path to import the script being tested
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
# pylint: disable=import-error,wrong-import-position
import verify_memory_accounting_coverage as vmac


# pylint: disable=invalid-name,unused-argument
class VerifyMemoryAccountingCoverageTest(unittest.TestCase):
  """Main test case for memory tracking coverage verification."""

  def setUp(self):
    self.maxDiff = None

  @patch("os.path.exists", return_value=True)
  def test_parse_uma_histos_valid(self, mock_exists):
    fake_file_content = (
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 1, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 12, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:05,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 2, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 40, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 3, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 15, '
        '"count": 3}]}}\n')
    with patch("builtins.open", return_value=io.StringIO(fake_file_content)):
      metrics = vmac.parse_uma_histos("dummy.txt")
      self.assertIn("Memory.Cobalt.AllocationVolume.Unknown", metrics)
      self.assertIn("Memory.Cobalt.AllocationVolume.Script", metrics)
      self.assertEqual(metrics["Memory.Cobalt.AllocationVolume.Unknown"][1], 12)
      self.assertEqual(metrics["Memory.Cobalt.AllocationVolume.Unknown"][3], 15)
      self.assertEqual(metrics["Memory.Cobalt.AllocationVolume.Script"][1], 40)

  @patch("os.path.exists", return_value=True)
  def test_parse_uma_histos_corrupted_json(self, mock_exists):
    fake_file_content = (
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 1, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 12, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:05,Memory.Cobalt.AllocationVolume.Script,"
        "{corrupted_json_payload}\n")
    with patch("builtins.open", return_value=io.StringIO(fake_file_content)):
      metrics = vmac.parse_uma_histos("dummy.txt")
      self.assertIn("Memory.Cobalt.AllocationVolume.Unknown", metrics)
      self.assertNotIn("Memory.Cobalt.AllocationVolume.Script", metrics)

  @patch("os.path.exists", return_value=True)
  def test_verify_accounting_success(self, mock_exists):
    # 15 seconds (count=3), kUnknown is 10 MB out of 100 MB (10%) -> PASS
    fake_file_content = (
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 1, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 10, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 2, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 90, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 3, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 10, '
        '"count": 3}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 4, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 90, '
        '"count": 3}]}}\n')
    with patch("builtins.open", return_value=io.StringIO(fake_file_content)):
      with patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:
        success = vmac.verify_accounting("dummy.txt")
        self.assertTrue(success)
        self.assertIn("[SUCCESS]", mock_stdout.getvalue())

  @patch("os.path.exists", return_value=True)
  def test_verify_accounting_failure(self, mock_exists):
    # 15 seconds (count=3), kUnknown is 30 MB out of 100 MB (30%) -> FAIL
    fake_file_content = (
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 1, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 10, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 2, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 90, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 3, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 30, '
        '"count": 3}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 4, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 70, '
        '"count": 3}]}}\n')
    with patch("builtins.open", return_value=io.StringIO(fake_file_content)):
      with patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:
        success = vmac.verify_accounting(
            "dummy.txt", cuj_name="Browse", iteration="1/4")
        self.assertFalse(success)
        self.assertIn("[OVERALL FAILURE]", mock_stdout.getvalue())
        self.assertIn("Failed Milestones: 15 Seconds", mock_stdout.getvalue())

  @patch("os.path.exists", return_value=True)
  def test_verify_accounting_pending_tolerance(self, mock_exists):
    # 15 seconds (count=3) passes at 10%, future milestones are PENDING
    fake_file_content = (
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 1, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 10, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:00,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 2, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 90, '
        '"count": 1}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Unknown,"
        '{"id": 3, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Unknown", "sum": 10, '
        '"count": 3}]}}\n'
        "2026-05-16 12:00:15,Memory.Cobalt.AllocationVolume.Script,"
        '{"id": 4, "result": {"histograms": [{"name": '
        '"Memory.Cobalt.AllocationVolume.Script", "sum": 90, '
        '"count": 3}]}}\n')
    with patch("builtins.open", return_value=io.StringIO(fake_file_content)):
      with patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:
        success = vmac.verify_accounting("dummy.txt")
        self.assertTrue(success)
        self.assertIn("[PENDING]", mock_stdout.getvalue())
        self.assertIn("[SUCCESS]", mock_stdout.getvalue())


if __name__ == "__main__":
  unittest.main()
