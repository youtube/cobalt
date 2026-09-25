# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Manages execution, logging, and process lifecycle of Cobalt instances."""

import logging
import os
import signal
import subprocess
import time
from typing import List, Optional, Union

logger = logging.getLogger('cobalt_runner')


class CobaltRunner:
  """Manages execution and lifecycle signaling of Cobalt instances."""

  def __init__(self,
               executable: Union[str, List[str]],
               log_file: str,
               port: int = 9223):
    """Initializes the CobaltRunner.

    Args:
      executable: Path to the executable binary or command list.
      log_file: Path to redirect stdout/stderr output.
      port: DevTools remote debugging port.
    """
    self.executable_cmd = executable
    self.log_file = log_file
    self.port = port
    self.proc: Optional[subprocess.Popen] = None
    self.log_handle = None

  def cleanup_existing(self) -> None:
    """Kills orphaned cobalt processes to prevent port/resource conflicts."""
    logger.info('Cleaning up any orphaned Cobalt processes...')
    for proc_name in ['cobalt_loader', 'loader_app', 'wait_for_state.sh']:
      subprocess.run(
          ['pkill', '-9', '-f', proc_name],
          stdout=subprocess.DEVNULL,
          stderr=subprocess.DEVNULL,
          check=False,
      )
    time.sleep(0.5)

  def launch(self, extra_args: List[str]) -> int:
    """Launches Cobalt in a background process.

    Args:
      extra_args: Additional command-line flags to pass to Cobalt.

    Returns:
      The process ID (PID) of the launched Cobalt process.

    Raises:
      Exception: If process launch fails.
    """
    self.cleanup_existing()
    if os.path.exists(self.log_file):
      os.remove(self.log_file)

    cmd = (
        self.executable_cmd if isinstance(self.executable_cmd, list) else
        self.executable_cmd.split())
    cmd = list(cmd) + extra_args

    logger.info('Launching Cobalt: %s', ' '.join(cmd))
    env = os.environ.copy()
    if 'ASAN_OPTIONS' not in env:
      env['ASAN_OPTIONS'] = 'detect_leaks=0'
    # pylint: disable=consider-using-with
    self.log_handle = open(self.log_file, 'w', encoding='utf-8')
    try:
      self.proc = subprocess.Popen(
          cmd,
          stdout=self.log_handle,
          stderr=subprocess.STDOUT,
          start_new_session=True,
          env=env,
      )
    except Exception:
      self.log_handle.close()
      self.log_handle = None
      raise
    logger.info('Launched PID: %d. Output: %s', self.proc.pid, self.log_file)
    return self.proc.pid

  def wait_for_exit(self, timeout: float = 15.0) -> int:
    """Waits for Cobalt to exit cleanly, raising an error on crash or timeout.

    Args:
      timeout: Seconds to wait before escalating to SIGKILL and failing.

    Returns:
      The exit code of the process (0 on clean exit).

    Raises:
      AssertionError: If Cobalt was not launched, hangs on exit, exits with a
        non-zero exit code, or logs a fatal crash marker.
    """
    if not self.proc:
      raise AssertionError('Cobalt process was never launched')

    start = time.time()
    ret = self.proc.poll()
    while ret is None and (time.time() - start < timeout):
      time.sleep(0.2)
      ret = self.proc.poll()

    if self.log_handle:
      self.log_handle.close()
      self.log_handle = None

    if ret is None:
      logger.error('Cobalt hung on exit after %ss. Escalating to SIGKILL...',
                   timeout)
      self._kill_process_group()
      raise AssertionError(
          f'Cobalt hung on exit and did not stop within {timeout}s')

    logger.info('Cobalt exited cleanly with code: %d', ret)
    if ret != 0:
      raise AssertionError(
          f'Cobalt crashed or exited uncleanly with non-zero code {ret}')

    logs = self.get_log_content()
    for crash_marker in (
        'ERROR: AddressSanitizer:',
        'SUMMARY: AddressSanitizer:',
        'Received signal ',
        'Check failed:',
        'FATAL:',
    ):
      if crash_marker in logs:
        raise AssertionError(
            f'Crash marker "{crash_marker}" detected in Cobalt logs')
    return ret

  def _kill_process_group(self) -> None:
    """Kills the launched process group or process with SIGKILL."""
    if not self.proc:
      return
    try:
      os.killpg(os.getpgid(self.proc.pid), signal.SIGKILL)
    except ProcessLookupError:
      pass
    except OSError:
      try:
        os.kill(self.proc.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass

  def get_log_content(self) -> str:
    """Reads and returns the complete log content."""
    if os.path.exists(self.log_file):
      with open(self.log_file, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()
    return ''

  def close(self) -> None:
    """Ensures process is terminated and log file handle is closed."""
    if self.proc and self.proc.poll() is None:
      self._kill_process_group()
    if self.log_handle:
      self.log_handle.close()
      self.log_handle = None

  def __enter__(self) -> 'CobaltRunner':
    return self

  def __exit__(self, exc_type, exc_val, exc_tb) -> None:
    self.close()
