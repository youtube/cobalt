"""Tests for check_coverage.py."""

import unittest
from unittest import mock
import sys
import os
import subprocess

from cobalt.tools.code_coverage.check_coverage import get_absolute_coverage, get_differential_coverage, main

sys.path.insert(
    0,
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))


class TestGetAbsoluteCoverage(unittest.TestCase):
  """Tests for the get_absolute_coverage function."""

  @mock.patch('subprocess.run')
  def test_successful_lcov_parsing(self, mock_subprocess):
    """Test that lcov output is parsed correctly."""
    mock_subprocess.return_value.stdout = 'lines......: 95.0% (19 of 20 lines)'
    self.assertEqual(get_absolute_coverage('dummy.lcov'), 95.0)

  @mock.patch('subprocess.run')
  def test_fallback_lcov_parsing(self, mock_subprocess):
    """Test the fallback parsing method for older lcov versions."""
    mock_subprocess.side_effect = subprocess.CalledProcessError(1, 'cmd')
    with mock.patch(
        'builtins.open',
        mock.mock_open(
            read_data='DA:1,1\nDA:2,0\nDA:3,1\nDA:4,1')) as mock_file:
      self.assertEqual(get_absolute_coverage('dummy.lcov'), 75.0)
      mock_file.assert_called_with('dummy.lcov', 'r', encoding='utf-8')

  @mock.patch('shutil.which', return_value=None)
  def test_lcov_not_found(self, mock_which):
    """Test that 0.0 is returned when lcov is not found."""
    del mock_which  # Unused argument
    self.assertEqual(get_absolute_coverage('dummy.lcov'), 0.0)

  @mock.patch(
      'subprocess.run', side_effect=subprocess.CalledProcessError(1, 'cmd'))
  def test_lcov_error(self, mock_subprocess):
    """Test that the fallback is used when lcov command fails."""
    del mock_subprocess  # Unused argument
    with mock.patch('builtins.open',
                    mock.mock_open(read_data='DA:1,1\nDA:2,0\nDA:3,1\nDA:4,1')):
      self.assertEqual(get_absolute_coverage('dummy.lcov'), 75.0)


class TestGetDifferentialCoverage(unittest.TestCase):
  """Tests for the get_differential_coverage function."""

  @mock.patch('subprocess.run')
  def test_successful_diff_cover_parsing(self, mock_subprocess):
    """Test that diff-cover JSON output is parsed correctly."""
    mock_subprocess.return_value.stdout = '{"percent_covered": 85.0}'
    self.assertEqual(get_differential_coverage('origin/main'), 85.0)

  @mock.patch('shutil.which', return_value=None)
  def test_diff_cover_not_found(self, mock_which):
    """Test that 0.0 is returned when diff-cover is not found."""
    del mock_which  # Unused argument
    self.assertEqual(get_differential_coverage('origin/main'), 0.0)

  @mock.patch(
      'subprocess.run', side_effect=subprocess.CalledProcessError(1, 'cmd'))
  def test_diff_cover_error(self, mock_subprocess):
    """Test that 0.0 is returned when diff-cover command fails."""
    del mock_subprocess  # Unused argument
    self.assertEqual(get_differential_coverage('origin/main'), 0.0)

  @mock.patch('subprocess.run')
  def test_diff_cover_invalid_json(self, mock_subprocess):
    """Test that 0.0 is returned when diff-cover output is not valid JSON."""
    mock_subprocess.return_value.stdout = 'invalid json'
    self.assertEqual(get_differential_coverage('origin/main'), 0.0)


class TestMain(unittest.TestCase):
  """Tests for the main function."""

  @mock.patch('sys.argv', ['script.py', '--input', 'report.lcov'])
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_absolute_coverage',
      return_value=95.0)
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_differential_coverage',
      return_value=85.0)
  def test_main_function_pass(self, mock_diff, mock_abs):
    """Test the main function with passing coverage."""
    del mock_diff, mock_abs  # Unused arguments
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, None)

  @mock.patch('sys.argv',
              ['script.py', '--input', 'report.lcov', '--fail-under', '90.0'])
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_absolute_coverage',
      return_value=95.0)
  @mock.patch(
      'cobalt.tools.code_coverage.check_coverage.get_differential_coverage',
      return_value=85.0)
  def test_main_function_fail(self, mock_diff, mock_abs):
    """Test the main function with failing coverage."""
    del mock_diff, mock_abs  # Unused arguments
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, 1)

  @mock.patch('sys.argv', ['script.py', '--input', 'non_existent.lcov'])
  def test_main_function_file_not_found(self):
    """Test that the main function exits if the input file is not found."""
    with self.assertRaises(SystemExit) as cm:
      main()
    self.assertEqual(cm.exception.code, 1)


if __name__ == '__main__':
  unittest.main()
