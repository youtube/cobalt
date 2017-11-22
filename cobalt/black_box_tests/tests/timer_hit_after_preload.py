"""Set a JS timer that expires after exiting preload mode."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests


class TimerAfterPreloadTest(black_box_tests.BlackBoxTestCase):

  def test_simple(self):

    url = self.GetURL(file_name='timer_hit_after_preload.html')

    with self.CreateCobaltRunner(
        url=url, target_params=['--preload']) as runner:
      self.assertTrue(runner.IsInPreload())
      # setInterval will hit once during the .5 seconds.
      runner.PollUntilFound('#script_executed')
      runner.SendResume()
      self.assertTrue(runner.HTMLTestsSucceeded())
