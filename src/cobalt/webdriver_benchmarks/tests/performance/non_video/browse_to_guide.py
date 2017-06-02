#!/usr/bin/python2
"""Simple benchmark for going from browse to guide and back."""

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
import tv
import tv_testcase
import tv_testcase_event_recorder
import tv_testcase_util

# selenium imports
keys = tv_testcase_util.import_selenium_module("webdriver.common.keys")

NUM_LOAD_TV_CALLS = 1
NUM_ITERATIONS_PER_LOAD_TV_CALL = 20

BROWSE_TO_GUIDE_EVENT_NAME = "wbBrowseToGuide"
BROWSE_TO_GUIDE_EVENT_TYPE = tv_testcase_util.EVENT_TYPE_KEY_DOWN

GUIDE_TO_BROWSE_EVENT_NAME = "wbGuideToBrowse"
GUIDE_TO_BROWSE_EVENT_TYPE = tv_testcase_util.EVENT_TYPE_KEY_DOWN


class BrowseToGuideTest(tv_testcase.TvTestCase):

  def test_simple(self):
    recorder_options = tv_testcase_event_recorder.EventRecorderOptions(
        self, BROWSE_TO_GUIDE_EVENT_NAME, BROWSE_TO_GUIDE_EVENT_TYPE)
    browse_to_guide_recorder = tv_testcase_event_recorder.EventRecorder(
        recorder_options)

    recorder_options = tv_testcase_event_recorder.EventRecorderOptions(
        self, GUIDE_TO_BROWSE_EVENT_NAME, GUIDE_TO_BROWSE_EVENT_TYPE)
    guide_to_browse_recorder = tv_testcase_event_recorder.EventRecorder(
        recorder_options)

    for _ in xrange(NUM_LOAD_TV_CALLS):
      self.load_tv()

      for _ in xrange(NUM_ITERATIONS_PER_LOAD_TV_CALL):
        browse_to_guide_recorder.on_start_event()
        self.send_keys(keys.Keys.ARROW_LEFT)
        self.wait_for_processing_complete()
        self.assert_displayed(tv.FOCUSED_GUIDE)
        browse_to_guide_recorder.on_end_event()

        guide_to_browse_recorder.on_start_event()
        self.send_keys(keys.Keys.ARROW_RIGHT)
        self.wait_for_processing_complete_after_focused_shelf()
        guide_to_browse_recorder.on_end_event()

    browse_to_guide_recorder.on_end_test()
    guide_to_browse_recorder.on_end_test()


if __name__ == "__main__":
  tv_testcase.main()
