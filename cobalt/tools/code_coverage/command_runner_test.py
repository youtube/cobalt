import unittest
import subprocess
import sys
from unittest import mock

# Add the project's root directory to the Python path.
import os
_PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, _PROJECT_ROOT)

from cobalt.tools.code_coverage.command_runner import run_command

class CommandRunnerTest(unittest.TestCase):

    @mock.patch('subprocess.Popen')
    def test_run_command_success(self, mock_popen):
        """Test a successful command execution."""
        mock_process = mock.Mock()
        mock_process.returncode = 0
        mock_process.stdout.readline.side_effect = ['output\n', '']
        mock_popen.return_value = mock_process

        run_command(['echo', 'hello'])
        mock_popen.assert_called_once_with(
            ['echo', 'hello'],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )

    @mock.patch('sys.exit')
    @mock.patch('subprocess.Popen')
    def test_run_command_failure_with_check(self, mock_popen, mock_exit):
        """Test a failing command with check=True exits."""
        mock_process = mock.Mock()
        mock_process.returncode = 1
        mock_process.stdout.readline.side_effect = ['error\n', '']
        mock_popen.return_value = mock_process

        run_command(['false'])
        mock_exit.assert_called_once_with(1)

    @mock.patch('sys.exit')
    @mock.patch('subprocess.Popen')
    def test_run_command_failure_without_check(self, mock_popen, mock_exit):
        """Test a failing command with check=False does not exit."""
        mock_process = mock.Mock()
        mock_process.returncode = 1
        mock_process.stdout.readline.side_effect = ['error\n', '']
        mock_popen.return_value = mock_process

        run_command(['false'], check=False)
        mock_exit.assert_not_called()

    @mock.patch('sys.exit')
    @mock.patch('subprocess.Popen', side_effect=FileNotFoundError("Command not found"))
    def test_run_command_file_not_found(self, mock_popen, mock_exit):
        """Test that a FileNotFoundError is caught and causes an exit."""
        run_command(['non_existent_command'])
        mock_exit.assert_called_once_with(1)

    @mock.patch('sys.exit')
    @mock.patch('subprocess.Popen', side_effect=Exception("Something broke"))
    def test_run_command_generic_exception(self, mock_popen, mock_exit):
        """Test that a generic exception is caught and causes an exit."""
        run_command(['any_command'])
        mock_exit.assert_called_once_with(1)

if __name__ == '__main__':
    unittest.main()
