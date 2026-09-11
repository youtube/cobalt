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

  def blur(self) -> None:
    logger.info('Sending SIGWINCH (BLUR)...')
    self.runner.send_signal(signal.SIGWINCH, 'SIGWINCH')

  def focus(self) -> None:
    logger.info('Sending SIGCONT (FOCUS)...')
    self.runner.send_signal(signal.SIGCONT, 'SIGCONT')

  def conceal(self) -> None:
    logger.info('Sending SIGUSR1 (CONCEAL)...')
    self.runner.send_signal(signal.SIGUSR1, 'SIGUSR1')

  def freeze(self) -> None:
    logger.info('Sending SIGTSTP (FREEZE)...')
    self.runner.send_signal(signal.SIGTSTP, 'SIGTSTP')

  def resume(self) -> None:
    logger.info('Sending SIGCONT (RESUME)...')
    self.runner.send_signal(signal.SIGCONT, 'SIGCONT')

  def stop(self) -> None:
    logger.info('Sending SIGPWR (STOP)...')
    self.runner.send_signal(signal.SIGPWR, 'SIGPWR')

  def low_memory(self) -> None:
    logger.info('Sending SIGUSR2 (LOW_MEMORY)...')
    self.runner.send_signal(signal.SIGUSR2, 'SIGUSR2')
