#!/usr/bin/python2
"""Simple shelf navigation test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys

# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
import tv
import tv_testcase
import partial_layout_benchmark

# selenium imports
keys = partial_layout_benchmark.ImportSeleniumModule("webdriver.common.keys")

DEFAULT_SHELVES_COUNT = 10
SHELF_ITEMS_COUNT = 10


class ShelfTest(tv_testcase.TvTestCase):

  def test_simple(self):
    layout_times_us = []
    self.load_tv()
    self.assert_displayed(tv.FOCUSED_SHELF)

    for _ in xrange(DEFAULT_SHELVES_COUNT):
      self.send_keys(tv.FOCUSED_SHELF, keys.Keys.ARROW_DOWN)
      self.wait_for_layout_complete_after_focused_shelf()
      layout_times_us.append(self.get_keyup_layout_duration_us())

    for _ in xrange(SHELF_ITEMS_COUNT):
      self.send_keys(tv.FOCUSED_TILE, keys.Keys.ARROW_RIGHT)
      self.wait_for_layout_complete_after_focused_shelf()
      layout_times_us.append(self.get_keyup_layout_duration_us())

    self.record_result_percentile(
        "webdriverBenchmarkShelfLayout95thUsec", 0.95, layout_times_us)


if __name__ == "__main__":
  tv_testcase.main()
