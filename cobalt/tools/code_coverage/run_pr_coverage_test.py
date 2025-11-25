import unittest
import os
import sys
import subprocess
import pathlib
from unittest import mock

# Add the project's root directory to the Python path.
_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, _PROJECT_ROOT)

# Import the module under test
from cobalt.tools.code_coverage import run_pr_coverage


class RunPrCoverageTest(unittest.TestCase):

  @mock.patch('cobalt.tools.code_coverage.run_pr_coverage.check_dependencies')
  @mock.patch('cobalt.tools.code_coverage.run_pr_coverage.CoverageRunner')
  @mock.patch('os.chdir')
  @mock.patch('pathlib.Path')
  def test_main_logic(self, mock_path, mock_chdir, mock_coverage_runner,
                      mock_check_dependencies):
    """
    Test that the main function correctly initializes and runs the CoverageRunner.
    """
    mock_script_dir = mock.MagicMock()
    mock_src_root = mock.MagicMock()
    mock_path.return_value.parent.resolve.return_value = mock_script_dir
    mock_script_dir.__truediv__.return_value.__truediv__.return_value.__truediv__.return_value.resolve.return_value = mock_src_root

    with mock.patch('sys.argv', ['run_pr_coverage.py', 'my_test_target']):
      run_pr_coverage.main()

    mock_check_dependencies.assert_called_once()
    mock_chdir.assert_called_once_with(mock_src_root)
    mock_coverage_runner.assert_called_once_with('my_test_target',
                                                 mock_src_root, mock_script_dir)
    mock_coverage_runner.return_value.run.assert_called_once()


class CoverageRunnerTest(unittest.TestCase):

  @mock.patch('cobalt.tools.code_coverage.run_pr_coverage.run_command')
  @mock.patch('sys.exit')
  def test_run_success(self, mock_sys_exit, mock_run_command):
    """
    Test the successful run of the CoverageRunner.
    """
    mock_src_root = mock.MagicMock(spec=pathlib.Path)
    mock_script_dir = mock.MagicMock(spec=pathlib.Path)
    lcov_file_mock = mock.MagicMock(spec=pathlib.Path)

    mock_src_root.__truediv__.return_value.__truediv__.return_value.__truediv__.return_value = lcov_file_mock
    lcov_file_mock.exists.return_value = True

    runner = run_pr_coverage.CoverageRunner(
        'my_target', src_root=mock_src_root, script_dir=mock_script_dir)
    runner.run()

    self.assertEqual(mock_run_command.call_count, 2)
    mock_sys_exit.assert_not_called()

  @mock.patch('cobalt.tools.code_coverage.run_pr_coverage.run_command')
  @mock.patch('sys.exit', side_effect=SystemExit)
  def test_run_fails_if_lcov_file_is_missing(self, mock_sys_exit,
                                             mock_run_command):
    """
    Test that the script exits if the LCOV file is missing.
    """
    mock_src_root = mock.MagicMock(spec=pathlib.Path)
    mock_script_dir = mock.MagicMock(spec=pathlib.Path)
    lcov_file_mock = mock.MagicMock(spec=pathlib.Path)

    mock_src_root.__truediv__.return_value.__truediv__.return_value.__truediv__.return_value = lcov_file_mock
    lcov_file_mock.exists.return_value = False  # Simulate missing file

    runner = run_pr_coverage.CoverageRunner(
        'my_target', src_root=mock_src_root, script_dir=mock_script_dir)
    with self.assertRaises(SystemExit):
      runner.run()

    # _generate_lcov_report is called, but _check_coverage exits
    mock_run_command.assert_called_once()
    mock_sys_exit.assert_called_once_with(1)

  @mock.patch(
      'cobalt.tools.code_coverage.run_pr_coverage.run_command',
      side_effect=subprocess.CalledProcessError(1, 'cmd'))
  @mock.patch('sys.exit', side_effect=SystemExit)
  def test_run_fails_on_command_error(self, mock_sys_exit, mock_run_command):
    """
    Test that the script exits if a command fails.
    """
    mock_src_root = mock.MagicMock(spec=pathlib.Path)
    mock_script_dir = mock.MagicMock(spec=pathlib.Path)

    runner = run_pr_coverage.CoverageRunner(
        'my_target', src_root=mock_src_root, script_dir=mock_script_dir)
    with self.assertRaises(subprocess.CalledProcessError):
      runner.run()

    mock_run_command.assert_called_once()
    mock_sys_exit.assert_not_called()


if __name__ == "__main__":
  unittest.main()
