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

import shutil
import sys
import unittest
from pathlib import Path
from unittest import mock

import generate_coverage_report

# This is a bit of a hack to import the script to be tested.
sys.path.append(str(Path(__file__).parent))


class TestGenerateCoverageReport(unittest.TestCase):
  """Test cases for the generate_coverage_report.py script."""

  def setUp(self):
    self.test_dir = Path("/tmp/coverage_test")
    self.base_dir = self.test_dir
    self.lcov_src_dir = self.test_dir / "out" / "android-x86_coverage"
    self.gen_files_dir = self.test_dir / "out" / "coverage_android-x86"
    self.cleaned_dir = self.lcov_src_dir / "cleaned"

    # Create all necessary directories
    self.cleaned_dir.mkdir(parents=True, exist_ok=True)
    self.gen_files_dir.mkdir(parents=True, exist_ok=True)

    # Override the constants in the script
    generate_coverage_report.BASE_DIR = self.base_dir
    generate_coverage_report.LCOV_SRC_DIR = self.lcov_src_dir
    generate_coverage_report.GEN_FILES_DIR = self.gen_files_dir

  def tearDown(self):
    shutil.rmtree(self.test_dir)

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

  @mock.patch("generate_coverage_report.normalize_lcov_paths")
  def test_simple_mock(self, mock_normalize):
    """
A simple test to verify mocking.
"""
    generate_coverage_report.normalize_lcov_paths(Path("foo"))
    mock_normalize.assert_called_once_with(Path("foo"))
