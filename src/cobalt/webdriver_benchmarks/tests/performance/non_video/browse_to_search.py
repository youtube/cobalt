#!/usr/bin/python2
"""Simple benchmark for going from browse to search and back."""

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

BROWSE_TO_SEARCH_EVENT_NAME = "wbBrowseToSearch"
BROWSE_TO_SEARCH_EVENT_TYPE = tv_testcase_util.EVENT_TYPE_KEY_UP

SEARCH_TO_BROWSE_EVENT_NAME = "wbSearchToBrowse"
SEARCH_TO_BROWSE_EVENT_TYPE = tv_testcase_util.EVENT_TYPE_KEY_UP


class BrowseToSearchTest(tv_testcase.TvTestCase):

  def test_simple(self):
    recorder_options = tv_testcase_event_recorder.EventRecorderOptions(
        self, BROWSE_TO_SEARCH_EVENT_NAME, BROWSE_TO_SEARCH_EVENT_TYPE)
    recorder_options.record_rasterize_animations = False
    browse_to_search_recorder = tv_testcase_event_recorder.EventRecorder(
        recorder_options)

    recorder_options = tv_testcase_event_recorder.EventRecorderOptions(
        self, SEARCH_TO_BROWSE_EVENT_NAME, SEARCH_TO_BROWSE_EVENT_TYPE)
    recorder_options.record_rasterize_animations = False
    search_to_browse_recorder = tv_testcase_event_recorder.EventRecorder(
        recorder_options)

    for _ in xrange(NUM_LOAD_TV_CALLS):
      self.load_tv()

      for _ in xrange(NUM_ITERATIONS_PER_LOAD_TV_CALL):
        browse_to_search_recorder.on_start_event()
        self.send_keys("s")
        self.wait_for_processing_complete(False)
        self.assert_displayed(tv.FOCUSED_SEARCH)
        browse_to_search_recorder.on_end_event()

        search_to_browse_recorder.on_start_event()
        self.send_keys(keys.Keys.ESCAPE)
        self.wait_for_processing_complete_after_focused_shelf()
        search_to_browse_recorder.on_end_event()

      browse_to_search_recorder.on_end_test()
      search_to_browse_recorder.on_end_test()


if __name__ == "__main__":
  tv_testcase.main()
