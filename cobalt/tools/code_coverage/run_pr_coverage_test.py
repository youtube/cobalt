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

  @mock.patch('subprocess.Popen')
  @mock.patch('os.chdir')
  @mock.patch('pathlib.Path.exists', return_value=True)
  def test_main_success_flow(self, mock_exists, mock_chdir, mock_popen):
    """
        Test the main function for a successful run, verifying all commands.
        """
    # Mock the process to simulate successful command execution
    mock_process = mock.Mock()
    mock_process.returncode = 0
    # Simulate stdout streaming
    mock_process.stdout.readline.side_effect = ['output\n', '']
    mock_popen.return_value = mock_process

    run_pr_coverage.main()

    # Verify that we changed to the correct directory
    mock_chdir.assert_called_once_with(run_pr_coverage.SRC_ROOT)

    # Verify that all three main commands were called
    self.assertEqual(mock_popen.call_count, 3)
    calls = mock_popen.call_args_list

    # 1. Verify pip install command
    pip_cmd = calls[0].args[0]
    self.assertIn("pip", pip_cmd[2])
    self.assertIn("install", pip_cmd[3])
    self.assertIn("diff-cover", pip_cmd[4])

    # 2. Verify coverage.py command
    coverage_cmd = ' '.join(calls[1].args[0])
    self.assertIn("tools/code_coverage/coverage.py", coverage_cmd)
    self.assertIn(run_pr_coverage.TARGET_TO_COVERAGE, coverage_cmd)
    self.assertIn("--format=lcov", coverage_cmd)

    # 3. Verify check_coverage.py command
    check_cmd = ' '.join(calls[2].args[0])
    self.assertIn("check_coverage.py", check_cmd)
    self.assertIn(f"--input {run_pr_coverage.SRC_ROOT / run_pr_coverage.LCOV_FILE}", check_cmd)
    self.assertIn("--fail-under 80.0", check_cmd)

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
