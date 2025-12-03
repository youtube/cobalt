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
"""Tests the automated code coverage checking process"""

import unittest
import json
import os
import subprocess
from cobalt.tools.code_coverage.check_coverage import (
    get_absolute_coverage, get_differential_coverage, main)
from unittest import mock


class CheckCoverageTest(unittest.TestCase):

  def setUp(self):
    """Set up test environment."""
    self.lcov_file = 'test.lcov'
    with open(self.lcov_file, 'w', encoding='utf-8') as f:
      f.write('test data')
    self.report_file = 'coverage_summary.md'
    self.json_report = 'diff_report.json'

  def tearDown(self):
    """Clean up test artifacts."""
    for f in [
        self.lcov_file, self.report_file, self.json_report,
        'coverage_summary.md'
    ]:
      if os.path.exists(f):
        os.remove(f)

  @mock.patch('subprocess.run')
  def test_get_absolute_coverage_success(self, mock_subprocess):
    """Test successful parsing of lcov output."""
    mock_subprocess.return_value = mock.Mock(
        stdout='lines......: 95.5% (123 of 456 lines)', returncode=0)
    coverage = get_absolute_coverage(self.lcov_file)
    self.assertAlmostEqual(coverage, 95.5)

  @mock.patch('shutil.which', return_value=None)
  def test_get_absolute_coverage_lcov_not_found(self, mock_which):
    """Test handling of lcov not being installed."""
    del mock_which  # Unused argument
    with self.assertRaises(SystemExit) as cm:
      get_absolute_coverage(self.lcov_file)
    self.assertEqual(cm.exception.code, 1)

  @mock.patch('shutil.which', return_value='/usr/bin/lcov')
  @mock.patch(
      'subprocess.run',
      side_effect=subprocess.CalledProcessError(1, 'cmd', 'error'))
  def test_get_absolute_coverage_process_error(self, mock_subprocess,
                                               mock_which):
    """Test handling of a CalledProcessError from lcov."""
    del mock_which, mock_subprocess  # Unused arguments
    with self.assertRaises(SystemExit) as cm:
      get_absolute_coverage(self.lcov_file)
    self.assertEqual(cm.exception.code, 1)

  @mock.patch('shutil.which', return_value='/usr/bin/lcov')
  @mock.patch('subprocess.run')
  def test_get_absolute_coverage_no_match(self, mock_subprocess, mock_which):
    """Test lcov output that doesn't match the regex."""
    del mock_which  # Unused argument
    mock_subprocess.return_value = mock.Mock(
        stdout='Some unexpected output from lcov', returncode=0)
    coverage = get_absolute_coverage(self.lcov_file)
    self.assertAlmostEqual(coverage, 0.0)

  @mock.patch('shutil.which', return_value='/usr/bin/diff-cover')
  @mock.patch('subprocess.run')
  def test_get_differential_coverage_success(self, mock_subprocess, mock_which):
    """Test successful differential coverage calculation."""
    del mock_which  # Unused argument
    mock_subprocess.return_value = mock.Mock(returncode=0)
    with open(self.json_report, 'w', encoding='utf-8') as f:
      json.dump({'coverage': '85.0'}, f)

    coverage = get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertAlmostEqual(coverage, 85.0)

  @mock.patch('shutil.which', return_value=None)
  def test_get_differential_coverage_diff_cover_not_found(self, mock_which):
    """Test handling of diff-cover not being installed."""
    del mock_which  # Unused argument
    with self.assertRaises(SystemExit) as cm:
      get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertEqual(cm.exception.code, 1)

  @mock.patch('shutil.which', return_value='/usr/bin/diff-cover')
  @mock.patch(
      'subprocess.run',
      side_effect=subprocess.CalledProcessError(1, 'cmd', 'error'))
  def test_get_differential_coverage_process_error(self, mock_subprocess,
                                                   mock_which):
    """Test that a process error from diff-cover defaults to 100%."""
    del mock_which, mock_subprocess  # Unused arguments
    coverage = get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertAlmostEqual(coverage, 100.0)

  @mock.patch('shutil.which', return_value='/usr/bin/diff-cover')
  @mock.patch('subprocess.run')
  def test_get_differential_coverage_no_report_generated(
      self, mock_subprocess, mock_which):
    """Test that 100% is returned if diff-cover generates no report."""
    del mock_which  # Unused argument
    mock_subprocess.return_value = mock.Mock(returncode=0)
    # Ensure no json report file exists
    if os.path.exists(self.json_report):
      os.remove(self.json_report)

    coverage = get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertAlmostEqual(coverage, 100.0)

  @mock.patch('shutil.which', return_value='/usr/bin/diff-cover')
  @mock.patch('subprocess.run')
  def test_get_differential_coverage_fallback_json(self, mock_subprocess,
                                                   mock_which):
    """Test the fallback logic for nested diff-cover JSON reports."""
    del mock_which  # Unused argument
    mock_subprocess.return_value = mock.Mock(returncode=0)
    # Simulate an older, nested JSON structure
    diff_cover_report = {'diff_cover': {'coverage': '78.9'}}
    with open(self.json_report, 'w', encoding='utf-8') as f:
      json.dump(diff_cover_report, f)

    coverage = get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertAlmostEqual(coverage, 78.9)

  @mock.patch('shutil.which', return_value='/usr/bin/diff-cover')
  @mock.patch('subprocess.run')
  def test_get_differential_coverage_json_error(self, mock_subprocess,
                                                mock_which):
    """Test handling of a JSONDecodeError from a malformed report."""
    del mock_which  # Unused argument
    mock_subprocess.return_value = mock.Mock(returncode=0)
    with open(self.json_report, 'w', encoding='utf-8') as f:
      f.write('this is not json')

    coverage = get_differential_coverage(self.lcov_file, 'origin/main')
    self.assertAlmostEqual(coverage, 0.0)

  @mock.patch('sys.argv', ['check_coverage.py', '--input=test.lcov'])
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_absolute_coverage',
      return_value=95.0)
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_differential_coverage',
      return_value=85.0)
  def test_main_success(self, mock_diff, mock_abs):
    """Test main function with coverage above threshold."""
    del mock_diff, mock_abs  # Unused arguments
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, 0)
    self.assertTrue(os.path.exists(self.report_file))
    with open(self.report_file, 'r', encoding='utf-8') as f:
      self.assertIn('✅ Pass', f.read())

  @mock.patch('sys.argv',
              ['check_coverage.py', '--input=test.lcov', '--fail-under=90.0'])
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_absolute_coverage',
      return_value=95.0)
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_differential_coverage',
      return_value=85.0)
  def test_main_failure(self, mock_diff, mock_abs):
    """Test main function with coverage below threshold."""
    del mock_diff, mock_abs  # Unused arguments
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, 1)
    self.assertTrue(os.path.exists(self.report_file))
    with open(self.report_file, 'r', encoding='utf-8') as f:
      self.assertIn('❌ Fail', f.read())

  @mock.patch('sys.argv', ['check_coverage.py', '--input=non_existent.lcov'])
  def test_main_missing_input_file(self):
    """Test main function with a missing input file."""
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, 1)


if __name__ == '__main__':
  unittest.main()
