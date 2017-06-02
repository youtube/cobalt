#!/usr/bin/python2
"""Simple benchmark for measuring startup time."""

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
import timer
import tv_testcase
import tv_testcase_util

NUM_BLANK_TO_BROWSE_ITERATIONS = 10

LAUNCH_TO_BLANK = "wbStartupDurLaunchToBlankUs"
BLANK_TO_BROWSE = "wbStartupDurBlankToBrowseUs"


class StartupTest(tv_testcase.TvTestCase):

  def setUp(self):
    # Override TvTestCase's setUp() so that blank startup can first be measured.
    pass

  def test_simple(self):
    """This test tries to measure the startup time for the YouTube TV page.

    Specifically, this test uses the Cobalt CVal Cobalt.Lifetime, which gets
    updated ~60Hz on a best effort basis and is in microseconds, to determine
    "wbStartupDurLaunchToBlankUs" and uses Timer to determine
    "wbStartupDurBlankToBrowseUs".
    """

    dur_launch_to_blank_us = self.get_cval("Cobalt.Lifetime")

    # Call TvTestCase's setUp() now that the blank startup time has been
    # measured.
    super(StartupTest, self).setUp()

    # Blank to browser record strategies
    blank_to_browse_record_strategies = []
    blank_to_browse_record_strategies.append(
        tv_testcase_util.RecordStrategyMean())
    blank_to_browse_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(25))
    blank_to_browse_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(50))
    blank_to_browse_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(75))
    blank_to_browse_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(95))

    # Blank to browser recorder
    blank_to_browse_recorder = tv_testcase_util.ResultsRecorder(
        BLANK_TO_BROWSE, blank_to_browse_record_strategies)

    for _ in range(NUM_BLANK_TO_BROWSE_ITERATIONS):
      self.load_blank()
      with timer.Timer(BLANK_TO_BROWSE) as t:
        self.load_tv()
      blank_to_browse_recorder.collect_value(int(t.seconds_elapsed * 1000000))

    tv_testcase_util.record_test_result(LAUNCH_TO_BLANK, dur_launch_to_blank_us)
    blank_to_browse_recorder.on_end_test()


if __name__ == "__main__":
  tv_testcase.main()
