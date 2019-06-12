"""Flood the process with continue signals, ensure signal handler is robust"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests
import time
import signal


class TimerAfterPreloadTest(black_box_tests.BlackBoxTestCase):

  def test_simple(self):
    with self.CreateCobaltRunner(url='', target_params=[]) as runner:
      if not 'linux' in runner.platform:
        self.skipTest('This test needs POSIX system signal handlers')

      runner.WaitForUrlLoadedEvents()

      # About minimum duration to trigger the original crash issue consistently
      duration_seconds = 5
      interval_seconds = 0.0005
      start_time = time.time()
      while time.time() < (start_time + duration_seconds):
        runner.launcher.proc.send_signal(signal.SIGCONT)
        time.sleep(interval_seconds)
