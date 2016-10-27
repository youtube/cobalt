#!/usr/bin/python2
"""Simple guide navigation test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys

# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(
    os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
import tv
import tv_testcase
import partial_layout_benchmark

# selenium imports
keys = partial_layout_benchmark.ImportSeleniumModule("webdriver.common.keys")

REPEAT_COUNT = 10


class GuideTest(tv_testcase.TvTestCase):

  def test_simple(self):
    self.load_tv()
    self.assert_displayed(tv.FOCUSED_SHELF)

    print(str(self.get_webdriver().execute_script(
        "h5vcc.system.recordStats = true")))

    for _ in xrange(REPEAT_COUNT):
      self.send_keys(tv.FOCUSED_SHELF, keys.Keys.ARROW_LEFT)
      self.assert_displayed(tv.FOCUSED_GUIDE)
      self.wait_for_layout_complete()
      self.send_keys(tv.FOCUSED_GUIDE, keys.Keys.ARROW_RIGHT)
      self.poll_until_found(tv.FOCUSED_SHELF)
      self.assert_displayed(tv.FOCUSED_SHELF_TITLE)
      self.wait_for_layout_complete()

    self.record_results("GuideTest.test_simple")


if __name__ == "__main__":
  tv_testcase.main()
