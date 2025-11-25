import unittest
import os
import sys
import subprocess
from unittest import mock

# Add the project's root directory to the Python path.
_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, _PROJECT_ROOT)

from cobalt.tools.code_coverage import run_pr_coverage


class RunPrCoverageTest(unittest.TestCase):

  @mock.patch('sys.argv', ['run_pr_coverage.py'])
  @mock.patch('subprocess.Popen')
  @mock.patch('os.chdir')
  @mock.patch('pathlib.Path.exists', return_value=True)
  def test_main_success_flow_default_target(self, mock_exists, mock_chdir, mock_popen):
    """
        Test the main function with the default target (crypto_unittests).
        """
    mock_process = mock.Mock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = ['output\n', '']
    mock_popen.return_value = mock_process

    run_pr_coverage.main()
    self.assertEqual(mock_popen.call_count, 3)
    calls = mock_popen.call_args_list
    # Verify coverage.py command uses the default target
    coverage_cmd = ' '.join(calls[1].args[0])
    self.assertIn("cobalt_unittests", coverage_cmd)
    self.assertNotIn("crypto_unittests", coverage_cmd)

  @mock.patch('sys.argv', ['run_pr_coverage.py', 'url_unittests'])
  @mock.patch('subprocess.Popen')
  @mock.patch('os.chdir')
  @mock.patch('pathlib.Path.exists', return_value=True)
  def test_main_custom_target(self, mock_exists, mock_chdir, mock_popen):
    """
        Test the main function with a custom specified target.
        """
    mock_process = mock.Mock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = ['output\n', '']
    mock_popen.return_value = mock_process

    run_pr_coverage.main()
    self.assertEqual(mock_popen.call_count, 3)
    calls = mock_popen.call_args_list

    # Verify coverage.py command uses the custom target
    coverage_cmd = ' '.join(calls[1].args[0])
    self.assertIn("url_unittests", coverage_cmd)
    self.assertNotIn("crypto_unittests", coverage_cmd)

    # Verify check_coverage.py command uses the correct lcov file path
    check_cmd = ' '.join(calls[2].args[0])
    self.assertIn("out/lcov_report/url_unittests.lcov", check_cmd)

  @mock.patch('subprocess.Popen')
  @mock.patch('os.chdir')
  @mock.patch('pathlib.Path.exists', return_value=False)
  def test_main_fails_if_lcov_file_is_missing(self, mock_exists, mock_chdir,
                                              mock_popen):
    """
        Test that the script exits with an error if the LCOV file is not
        generated.
        """
    mock_process = mock.Mock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = ['output\n', '']
    mock_popen.return_value = mock_process

    with self.assertRaises(SystemExit) as cm:
      run_pr_coverage.main()

    self.assertEqual(cm.exception.code, 1)

  @mock.patch('subprocess.Popen')
  @mock.patch('os.chdir')
  def test_main_fails_on_command_error(self, mock_chdir, mock_popen):
    """
        Test that the script exits with an error if any command fails.
        """
    # Mock a failing process for the first command (pip install)
    mock_process = mock.Mock()
    mock_process.returncode = 1  # Simulate failure
    mock_process.stdout.readline.side_effect = ['output\n', '']
    mock_popen.return_value = mock_process

    with self.assertRaises(SystemExit) as cm:
      run_pr_coverage.main()

    self.assertEqual(cm.exception.code, 1)
    # Ensure we only tried to run the first command
    mock_popen.assert_called_once()


if __name__ == "__main__":
  unittest.main()
