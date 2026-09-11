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
      env['ASAN_OPTIONS'] = 'exitcode=0:detect_leaks=0'
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

  def send_signal(self, sig: int, name: Optional[str] = None) -> None:
    """Sends a signal to the Cobalt process.

    Args:
      sig: Signal number (e.g. signal.SIGPWR).
      name: Optional human-readable signal name.

    Raises:
      RuntimeError: If the Cobalt process is not currently running.
    """
    if not self.proc or self.proc.poll() is not None:
      raise RuntimeError('Cobalt process is not running!')
    sig_name = name or (sig.name if hasattr(sig, 'name') else str(sig))
    logger.info('Sending %s to PID %d...', sig_name, self.proc.pid)
    os.kill(self.proc.pid, sig)

  def wait_for_exit(self, timeout: float = 15.0) -> int:
    """Waits for Cobalt to exit cleanly, escalating to SIGKILL on timeout.

    Args:
      timeout: Seconds to wait before escalating to SIGKILL.

    Returns:
      The exit code of the process.

    Raises:
      AssertionError: If Cobalt exits with an unexpected error code.
    """
    if not self.proc or self.proc.poll() is not None:
      return 0 if not self.proc else (self.proc.poll() or 0)

    start = time.time()
    while time.time() - start < timeout:
      ret = self.proc.poll()
      if ret is not None:
        logger.info('Cobalt exited cleanly with code: %d', ret)
        if self.log_handle:
          self.log_handle.close()
          self.log_handle = None
        allowed_codes = (0, 143, -signal.SIGPWR, -signal.SIGTERM)
        if ret not in allowed_codes:
          raise AssertionError(
              f'Cobalt exited with unexpected non-zero code {ret}')
        return ret
      time.sleep(0.5)

    logger.warning('Cobalt did not stop after %ss. Escalating to SIGKILL...',
                   timeout)
    try:
      os.kill(self.proc.pid, signal.SIGKILL)
    except ProcessLookupError:
      pass
    if self.log_handle:
      self.log_handle.close()
      self.log_handle = None
    return -9

  def terminate_and_wait(self, timeout: float = 15.0) -> int:
    """Sends SIGPWR and waits for Cobalt to terminate cleanly.

    Args:
      timeout: Seconds to wait for termination.

    Returns:
      The exit code of the process.
    """
    if not self.proc or self.proc.poll() is not None:
      return 0 if not self.proc else (self.proc.poll() or 0)

    logger.info('Sending SIGPWR (graceful stop) to PID %d...', self.proc.pid)
    try:
      self.send_signal(signal.SIGPWR, 'SIGPWR')
    except ProcessLookupError:
      pass
    return self.wait_for_exit(timeout)

  def get_log_content(self) -> str:
    """Reads and returns the complete log content."""
    if os.path.exists(self.log_file):
      with open(self.log_file, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()
    return ''

  def close(self) -> None:
    """Ensures process is terminated and log file handle is closed."""
    if self.proc and self.proc.poll() is None:
      try:
        os.kill(self.proc.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass
    if self.log_handle:
      self.log_handle.close()
      self.log_handle = None

  def __enter__(self) -> 'CobaltRunner':
    return self

  def __exit__(self, exc_type, exc_val, exc_tb) -> None:
    self.close()
