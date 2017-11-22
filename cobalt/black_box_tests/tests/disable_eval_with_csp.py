"""Set a JS timer that expires after exiting preload mode."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests


class DisableEvalWithCSPTest(black_box_tests.BlackBoxTestCase):

  def test_simple(self):

    url = self.GetURL(file_name='disable_eval_with_csp.html')

    with self.CreateCobaltRunner(url=url) as runner:
      self.assertTrue(runner.HTMLTestsSucceeded())
