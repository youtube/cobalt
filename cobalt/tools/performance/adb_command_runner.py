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
"""ADB command runner that accepts an ADB command and interacts with the
   subprocess library to execute the commands.
   This provides three outputs to indicate the success of the command:
   stdout, stderr, and the return code.
"""

import subprocess
from typing import List, Optional, Tuple, Union, Callable

# Define a type hint for the subprocess runner for clarity
SubprocessRunner = Callable[
    [Union[List[str], str], bool, int],
    Tuple[str, str, int]  # stdout, stderr, return code
]


def _run_subprocess_raw(command_list_or_str: Union[List[str], str], shell: bool,
                        timeout: int) -> Tuple[str, str, int]:
  """Internal helper to run a subprocess command and return raw output.

  This function is designed to be easily mockable or swappable for testing.
  It intentionally does *not* handle application-specific error parsing or
  timeout messages, focusing solely on the subprocess execution itself.
  """
  try:
    with subprocess.Popen(
        command_list_or_str,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors='replace',
        encoding='utf-8',
    ) as process:
      stdout_bytes, stderr_bytes = process.communicate(timeout=timeout)
      stdout = stdout_bytes.strip() if stdout_bytes else ''
      stderr = stderr_bytes.strip() if stderr_bytes else ''
      return stdout, stderr, process.returncode
  except subprocess.TimeoutExpired:
    # Kill the process if it timed out and is still running
    if 'process' in locals() and hasattr(process,
                                         'poll') and process.poll() is None:
      process.kill()
      process.communicate()  # Wait for the process to terminate
    # For a raw runner, we indicate timeout by returning a specific return code
    # or a clear error string. Here, we'll use a high return code and error.
    return '', 'Command timed out', -1  # Indicate timeout with -1 return code
  except Exception as e:  # pylint: disable=broad-exception-caught
    return '', str(e), -2  # Indicate generic error with -2 return code


def _determine_command_to_run(command_list_or_str: Union[List[str], str],
                              shell: bool) -> Union[List[str], str]:
  """Helper to determine the actual command format for subprocess."""
  if shell and isinstance(command_list_or_str, list):
    return ' '.join(command_list_or_str)
  return command_list_or_str


def _is_expected_adb_failure(cmd_str_repr: str, return_code: int,
                             stderr: str) -> bool:
  """Checks if a given ADB command failure is expected."""
  is_expected_pidof_fail = ('pidof' in cmd_str_repr and return_code == 1)
  is_expected_meminfo_fail = ('dumpsys meminfo' in cmd_str_repr and
                              ('No process found' in stderr or
                               'No services match' in stderr))
  is_expected_sf_fail = ('dumpsys SurfaceFlinger' in cmd_str_repr and
                         'service not found' in stderr.lower())
  return (is_expected_pidof_fail or is_expected_meminfo_fail or
          is_expected_sf_fail)


def run_adb_command(
    command_list_or_str: Union[List[str], str],
    shell: bool = False,
    timeout: int = 30,
    subprocess_runner:
    SubprocessRunner = _run_subprocess_raw  # Dependency Injection!
) -> Tuple[str, Optional[str]]:
  """Helper function to run ADB commands (refactored for testability).

  Args:
    command_list_or_str: A list of command arguments or a single string
                         command.
    shell: If True, the command is executed through the shell.
    timeout: Maximum time in seconds to wait for the command to complete.
    subprocess_runner: A callable that takes (command, shell, timeout)
                       and returns (stdout, stderr, returncode).
                       Defaults to the real subprocess execution.

  Returns:
    A tuple (stdout, stderr_msg). stdout is a string.
    stderr_msg is None on success, or an error message string on
    failure/timeout.
  """
  cmd_str_repr = (
      command_list_or_str if isinstance(command_list_or_str, str) else
      ' '.join(command_list_or_str))

  command_to_run = _determine_command_to_run(command_list_or_str, shell)

  stdout, stderr, return_code = subprocess_runner(command_to_run, shell,
                                                  timeout)

  if return_code == 0:
    return stdout, None
  elif return_code == -1:  # Indicates timeout from _run_subprocess_raw
    print(f'Command timed out: {cmd_str_repr}')
    return None, 'Command timed out'
  elif return_code == -2:  # Indicates generic error from _run_subprocess_raw
    print(f'Exception running command {cmd_str_repr}: {stderr}')
    return None, stderr
  else:  # Non-zero return code from the command itself
    if not _is_expected_adb_failure(cmd_str_repr, return_code,
                                    stderr) and stderr:
      print(f'Stderr for \'{cmd_str_repr}\': {stderr.strip()}')
    return (
        stdout,
        stderr if stderr else f'Command failed with return code {return_code}',
    )
