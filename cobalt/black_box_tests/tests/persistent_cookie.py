"""Open Cobalt in preload mode and find a basic element."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests
from cobalt.tools.automated_testing import webdriver_utils

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')

_MAX_RESUME_WAIT_SECONDS = 30


class PersistentCookieTest(black_box_tests.BlackBoxTestCase):

  # The same page has to be used since cookie are stored per URL.

  def test_simple(self):

    url = self.GetURL(file_name='persistent_cookie.html')

    # The webpage listens for NUMPAD1, NUMPAD2 and NUMPAD3 at opening.
    with self.CreateCobaltRunner(url=url) as runner:
      # Press NUMPAD1 to verify basic cookie functionality and set
      # a persistent cookie.
      runner.SendKeys(keys.Keys.NUMPAD1)
      self.assertTrue(runner.HTMLTestsSucceeded())

    with self.CreateCobaltRunner(url=url) as runner:
      # Press NUMPAD2 to indicate this is the second time we opened
      # the webpage and verify a persistent cookie is on device. Then
      # clear this persistent cookie.
      runner.SendKeys(keys.Keys.NUMPAD2)
      self.assertTrue(runner.HTMLTestsSucceeded())

    with self.CreateCobaltRunner(url=url) as runner:
      # Press NUMPAD3 to verify the persistent cookie we cleared is
      # not on the device for this URL any more.
      runner.SendKeys(keys.Keys.NUMPAD3)
      self.assertTrue(runner.HTMLTestsSucceeded())
