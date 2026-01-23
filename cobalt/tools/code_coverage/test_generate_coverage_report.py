#!/usr/bin/env python3
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
"""Tests code coverage report generation for Cobalt."""

import sys
import unittest
import tempfile
from pathlib import Path
from unittest import mock

import generate_coverage_report

# This is a bit of a hack to import the script to be tested.
sys.path.append(str(Path(__file__).parent))


class TestGenerateCoverageReport(unittest.TestCase):
  """Test cases for the generate_coverage_report.py script."""

  def setUp(self):
    # pylint: disable=consider-using-with
    self.test_dir_obj = tempfile.TemporaryDirectory()
    self.addCleanup(self.test_dir_obj.cleanup)
    self.test_dir = Path(self.test_dir_obj.name)
    self.base_dir = self.test_dir
    self.platform = "android-x86"
    self.lcov_src_dir = self.base_dir / "out" / f"{self.platform}_coverage"
    self.cleaned_dir = self.lcov_src_dir / "cleaned"

    # Create all necessary directories
    self.cleaned_dir.mkdir(parents=True, exist_ok=True)

  def tearDown(self):
    pass

  def test_normalize_lcov_paths(self):
    """
    Tests that the path normalization function correctly rewrites paths
    for both regular and generated source files.
    """
    lcov_content = """TN:
SF:../../cobalt/browser/browser.cc
DA:1,1
DA:2,1
SF:gen/cobalt/bindings/v8c/testing/generated.cc
DA:10,1
DA:11,0
SF:../../third_party/some_lib/lib.cc
DA:1,1
end_of_record
"""
    lcov_file = self.cleaned_dir / "test.lcov"
    lcov_file.write_text(lcov_content)

    # Run the function to be tested
    generate_coverage_report.normalize_lcov_paths(lcov_file)

    # Read the modified file
    normalized_content = lcov_file.read_text()

    # Note: gen/ files are SKIPPED entirely, including their data lines.
    expected_content = """TN:
SF:cobalt/browser/browser.cc
DA:1,1
DA:2,1
SF:third_party/some_lib/lib.cc
DA:1,1
end_of_record
"""

    self.assertEqual(normalized_content.strip(), expected_content.strip())

  @mock.patch("generate_coverage_report.run_command")
  def test_generate_report_no_lcov_files(self, mock_run):
    """Tests generate_report when no lcov files are found."""
    # Ensure lcov_src_dir exists but is empty
    self.lcov_src_dir.mkdir(parents=True, exist_ok=True)

    result = generate_coverage_report.generate_report(self.platform,
                                                      self.base_dir)
    self.assertFalse(result)
    mock_run.assert_not_called()

  @mock.patch("generate_coverage_report.run_command")
  @mock.patch("generate_coverage_report.normalize_lcov_paths")
  def test_generate_report_success(self, mock_normalize, mock_run):
    """Tests a successful run of generate_report."""
    # Setup: Create a dummy lcov file
    dummy_lcov = self.lcov_src_dir / "test.lcov"
    dummy_lcov.parent.mkdir(parents=True, exist_ok=True)
    dummy_lcov.write_text("dummy lcov content")

    # Mock run_command to always return True
    mock_run.return_value = True

    # Mock the cleaned file creation because lcov won't actually run
    cleaned_file = self.cleaned_dir / "test.lcov"

    def side_effect(command, cwd=None):  # pylint: disable=unused-argument
      if "--extract" in command:
        cleaned_file.write_text("cleaned content")
      return True

    mock_run.side_effect = side_effect

    result = generate_coverage_report.generate_report(self.platform,
                                                      self.base_dir)

    self.assertTrue(result)
    self.assertTrue(mock_run.called)
    self.assertTrue(mock_normalize.called)
    # Check if final lcov file was "created" (it wouldn't be by mock,
    # but the command would have been called)
    merge_call = [
        call for call in mock_run.call_args_list
        if "lcov" in call.args[0] and "--add-tracefile" in call.args[0]
    ]
    self.assertTrue(len(merge_call) > 0)
