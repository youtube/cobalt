"""Set a JS timer that expires after exiting preload mode."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests


class SuspendVisibilityTest(black_box_tests.BlackBoxTestCase):

  def test_simple(self):

    url = self.GetURL(file_name='suspend_visibility.html')

    with self.CreateCobaltRunner(url=url) as runner:
      runner.PollUntilFound('#visibility_event_listener_added')
      runner.SendSuspend()
      runner.SendResume()
      self.assertTrue(runner.HTMLTestsSucceeded())
