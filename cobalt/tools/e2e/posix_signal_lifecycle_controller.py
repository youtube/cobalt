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
"""Controls application lifecycle via POSIX signals (suspend_signals.cc)."""

import logging
import os
import signal

try:
  from cobalt.tools.e2e.cobalt_runner import CobaltRunner
  from cobalt.tools.e2e.lifecycle_controller import LifecycleController
except ImportError:
  from cobalt_runner import CobaltRunner  # type: ignore[no-redef]
  from lifecycle_controller import LifecycleController  # type: ignore[no-redef]

logger = logging.getLogger('posix_signal_lifecycle_controller')


class PosixSignalLifecycleController(LifecycleController):
  """Controls application lifecycle via POSIX signals (suspend_signals.cc)."""

  def __init__(self, runner: CobaltRunner):
    self.runner = runner

  def _send_signal(self, sig: int, name: str) -> None:
    proc = self.runner.proc
    if not proc or proc.poll() is not None:
      raise RuntimeError('Cobalt process is not running!')
    logger.info('Sending %s to PID %d...', name, proc.pid)
    os.kill(proc.pid, sig)

  def blur(self) -> None:
    self._send_signal(signal.SIGWINCH, 'SIGWINCH')

  def focus(self) -> None:
    self._send_signal(signal.SIGCONT, 'SIGCONT')

  def conceal(self) -> None:
    self._send_signal(signal.SIGUSR1, 'SIGUSR1')

  def freeze(self) -> None:
    self._send_signal(signal.SIGTSTP, 'SIGTSTP')

  def resume(self) -> None:
    self._send_signal(signal.SIGCONT, 'SIGCONT')

  def stop(self) -> None:
    self._send_signal(signal.SIGPWR, 'SIGPWR')

  def low_memory(self) -> None:
    self._send_signal(signal.SIGUSR2, 'SIGUSR2')
