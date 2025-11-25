import unittest
import os
import json
import subprocess
import sys
from unittest import mock

# Add the project root to the path to allow importing 'cobalt'
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))

from cobalt.tools.check_coverage import parse_lcov_absolute_coverage, get_differential_coverage, main

class CheckCoverageTest(unittest.TestCase):

    def setUp(self):
        """Set up test environment."""
        self.lcov_file = "test.lcov"
        self.report_file = "coverage_report.md"
        self.json_report = "diff_cover_report.json"

    def tearDown(self):
        """Clean up test artifacts."""
        for f in [self.lcov_file, self.report_file, self.json_report]:
            if os.path.exists(f):
                os.remove(f)

    def test_parse_lcov_absolute_coverage_valid(self):
        """Test parsing a valid LCOV file."""
        lcov_content = """
TN:
SF:source/file1.cc
FN:10,(anonymous_namespace)::FunctionName
FNDA:1,(anonymous_namespace)::FunctionName
FNF:1
FNH:1
BRDA:10,0,0,1
BRF:1
BRH:1
DA:10,1
DA:11,1
LF:2
LH:2
end_of_record
SF:source/file2.cc
LF:8
LH:6
end_of_record
"""
        with open(self.lcov_file, 'w') as f:
            f.write(lcov_content)
        
        coverage = parse_lcov_absolute_coverage(self.lcov_file)
        # (2+6) / (2+8) = 80.0
        self.assertAlmostEqual(coverage, 80.0)

    def test_parse_lcov_absolute_coverage_no_lines(self):
        """Test LCOV parsing with zero lines found."""
        with open(self.lcov_file, 'w') as f:
            f.write("TN:\nend_of_record\n")
        coverage = parse_lcov_absolute_coverage(self.lcov_file)
        self.assertEqual(coverage, 0.0)

    def test_parse_lcov_absolute_coverage_file_not_found(self):
        """Test LCOV parsing when the file is missing."""
        with self.assertRaises(SystemExit) as cm:
            parse_lcov_absolute_coverage("non_existent_file.lcov")
        self.assertEqual(cm.exception.code, 1)

    def test_parse_lcov_absolute_coverage_no_hits(self):
        """Test LCOV parsing with lines found but no lines hit."""
        with open(self.lcov_file, 'w') as f:
            f.write("TN:\nLF:10\nLH:0\nend_of_record\n")
        coverage = parse_lcov_absolute_coverage(self.lcov_file)
        self.assertEqual(coverage, 0.0)

    @mock.patch('subprocess.run')
    def test_get_differential_coverage_success(self, mock_subprocess):
        """Test successful differential coverage calculation."""
        mock_subprocess.return_value = mock.Mock(returncode=0)
        diff_cover_report = {'coverage': '95.5'}
        with open(self.json_report, 'w') as f:
            json.dump(diff_cover_report, f)

        coverage = get_differential_coverage(self.lcov_file, "origin/main")
        self.assertAlmostEqual(coverage, 95.5)
        mock_subprocess.assert_called_once_with([
            "diff-cover",
            self.lcov_file,
            "--compare-branch=origin/main",
            f"--json-report={self.json_report}"
        ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    @mock.patch('subprocess.run')
    def test_get_differential_coverage_fallback_json(self, mock_subprocess):
        """Test the fallback logic for nested diff-cover JSON reports."""
        mock_subprocess.return_value = mock.Mock(returncode=0)
        # Simulate an older, nested JSON structure
        diff_cover_report = {
            "diff_cover": {
                "coverage": "78.9"
            }
        }
        with open(self.json_report, 'w') as f:
            json.dump(diff_cover_report, f)
        
        coverage = get_differential_coverage(self.lcov_file, "origin/main")
        self.assertAlmostEqual(coverage, 78.9)


    @mock.patch('subprocess.run', side_effect=subprocess.CalledProcessError(1, "cmd", b"stdout", b"stderr"))
    def test_get_differential_coverage_process_error(self, mock_subprocess):
        """Test handling of a CalledProcessError from diff-cover."""
        with self.assertRaises(SystemExit) as cm:
            get_differential_coverage(self.lcov_file, "origin/main")
        self.assertEqual(cm.exception.code, 1)

    @mock.patch('subprocess.run')
    def test_get_differential_coverage_json_error(self, mock_subprocess):
        """Test handling of a JSONDecodeError from a malformed report."""
        mock_subprocess.return_value = mock.Mock(returncode=0)
        with open(self.json_report, 'w') as f:
            f.write("this is not json")

        with self.assertRaises(SystemExit) as cm:
            get_differential_coverage(self.lcov_file, "origin/main")
        self.assertEqual(cm.exception.code, 1)

    @mock.patch('subprocess.run', side_effect=FileNotFoundError)
    def test_get_differential_coverage_command_not_found(self, mock_subprocess):
        """Test handling of FileNotFoundError when diff-cover is not installed."""
        with self.assertRaises(SystemExit) as cm:
            get_differential_coverage(self.lcov_file, "origin/main")
        self.assertEqual(cm.exception.code, 1)

    @mock.patch('subprocess.run', side_effect=Exception("Something broke"))
    def test_get_differential_coverage_unexpected_error(self, mock_subprocess):
        """Test handling of an unexpected exception."""
        with self.assertRaises(SystemExit) as cm:
            get_differential_coverage(self.lcov_file, "origin/main")
        self.assertEqual(cm.exception.code, 1)

    @mock.patch('sys.argv', ['check_coverage.py', '--lcov-file=test.lcov', '--threshold=75.0'])
    @mock.patch('cobalt.tools.check_coverage.parse_lcov_absolute_coverage', return_value=85.0)
    @mock.patch('cobalt.tools.check_coverage.get_differential_coverage', return_value=90.0)
    def test_main_success(self, mock_diff_cov, mock_abs_cov):
        """Test the main function with coverage above the threshold."""
        main()
        self.assertTrue(os.path.exists(self.report_file))
        with open(self.report_file, 'r') as f:
            content = f.read()
            self.assertIn("✅", content)

    @mock.patch('sys.argv', ['check_coverage.py', '--lcov-file=test.lcov', '--threshold=95.0'])
    @mock.patch('cobalt.tools.check_coverage.parse_lcov_absolute_coverage', return_value=85.0)
    @mock.patch('cobalt.tools.check_coverage.get_differential_coverage', return_value=90.0)
    def test_main_failure(self, mock_diff_cov, mock_abs_cov):
        """Test the main function with coverage below the threshold."""
        with self.assertRaises(SystemExit) as cm:
            main()
        self.assertEqual(cm.exception.code, 1)
        self.assertTrue(os.path.exists(self.report_file))
        with open(self.report_file, 'r') as f:
            content = f.read()
            self.assertIn("❌", content)

    @mock.patch('sys.argv', ['check_coverage.py', '--lcov-file=test.lcov', '--compare-branch=origin/develop', '--report-file=custom_report.md'])
    @mock.patch('cobalt.tools.check_coverage.parse_lcov_absolute_coverage', return_value=100.0)
    @mock.patch('cobalt.tools.check_coverage.get_differential_coverage', return_value=100.0)
    def test_main_argument_parsing(self, mock_diff_cov, mock_abs_cov):
        """Test that command-line arguments are correctly parsed."""
        main()
        mock_abs_cov.assert_called_once_with('test.lcov')
        mock_diff_cov.assert_called_once_with('test.lcov', 'origin/develop')
        self.assertTrue(os.path.exists('custom_report.md'))
        # Clean up the custom report file
        if os.path.exists('custom_report.md'):
            os.remove('custom_report.md')

if __name__ == "__main__":
    unittest.main()
