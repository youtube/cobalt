#!/usr/bin/python2
"""Simple benchmark of vertically navigating shelves during browse."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys

# Add the base webdriver_benchmarks path
sys.path.insert(0,
                os.path.dirname(
                    os.path.dirname((os.path.dirname(
                        os.path.dirname(os.path.realpath(__file__)))))))

# pylint: disable=C6204,C6203
import tv_testcase
import tv_testcase_event_recorder
import tv_testcase_util

# selenium imports
keys = tv_testcase_util.import_selenium_module("webdriver.common.keys")

NUM_LOAD_TV_CALLS = 4
NUM_ITERATIONS_PER_LOAD_TV_CALL = 25

BROWSE_VERTICAL_EVENT_NAME = "wbBrowseVertical"
BROWSE_VERTICAL_EVENT_TYPE = tv_testcase_util.EVENT_TYPE_KEY_DOWN


class BrowseVerticalTest(tv_testcase.TvTestCase):

  def test_simple(self):
    recorder_options = tv_testcase_event_recorder.EventRecorderOptions(
        self, BROWSE_VERTICAL_EVENT_NAME, BROWSE_VERTICAL_EVENT_TYPE)
    recorder = tv_testcase_event_recorder.EventRecorder(recorder_options)

    for _ in xrange(NUM_LOAD_TV_CALLS):
      self.load_tv()
      self.send_keys(keys.Keys.ARROW_DOWN)
      self.wait_for_processing_complete_after_focused_shelf()

      for _ in xrange(NUM_ITERATIONS_PER_LOAD_TV_CALL):
        recorder.on_start_event()
        self.send_keys(keys.Keys.ARROW_DOWN)
        self.wait_for_processing_complete_after_focused_shelf()
        recorder.on_end_event()

    recorder.on_end_test()


if __name__ == "__main__":
  tv_testcase.main()
