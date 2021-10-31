# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import signal
import time

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests


class SignalHandlerDoesntCrashTest(black_box_tests.BlackBoxTestCase):
  """Flood the process with CONT signals, ensuring signal handler is robust."""

  def setUp(self):
    if 'linux' not in self.launcher_params.platform:
      self.skipTest('This test needs POSIX system signal handlers')

  def test_simple(self):
    with self.CreateCobaltRunner(url='', target_params=[]) as runner:
      runner.WaitForUrlLoadedEvents()

      # About minimum duration to trigger the original crash issue consistently
      duration_seconds = 5
      interval_seconds = 0.0005
      start_time = time.time()
      while time.time() < (start_time + duration_seconds):
        runner.launcher.proc.send_signal(signal.SIGCONT)
        time.sleep(interval_seconds)
