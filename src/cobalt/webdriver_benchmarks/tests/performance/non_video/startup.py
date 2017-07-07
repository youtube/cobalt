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
import c_val_names
import tv_testcase
import tv_testcase_util

NUM_LOAD_TV_ITERATIONS = 20

STARTUP_RECORD_NAME = "wbStartup"


class StartupTest(tv_testcase.TvTestCase):

  def setUp(self):
    # Override TvTestCase's setUp() so that blank startup can first be measured.
    pass

  def test_simple(self):
    """This test tries to measure the startup time for the YouTube TV page."""
    self.wait_for_processing_complete()
    self.wait_for_html_script_element_execute_count(2)

    # Measure durations for the intial launch.
    launch_time = self.get_cval(c_val_names.time_cobalt_start())
    navigate_time = self.get_cval(c_val_names.time_browser_navigate())
    on_load_event_time = self.get_cval(c_val_names.time_browser_on_load_event())
    browse_time = self.get_cval(c_val_names.time_rasterize_new_render_tree())
    usable_time = self.get_cval(
        c_val_names.time_dom_html_script_element_execute())

    dur_launch_to_navigate_us = navigate_time - launch_time
    dur_launch_to_on_load_event_us = on_load_event_time - launch_time
    dur_launch_to_browse_us = browse_time - launch_time
    dur_launch_to_usable_us = usable_time - launch_time

    # Call TvTestCase's setUp() now that the launch times have been measured.
    super(StartupTest, self).setUp()

    # Count record strategies
    count_record_strategies = []
    count_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    count_record_strategies.append(tv_testcase_util.RecordStrategyMin())
    count_record_strategies.append(tv_testcase_util.RecordStrategyMedian())
    count_record_strategies.append(tv_testcase_util.RecordStrategyMax())

    # Duration record strategies
    duration_record_strategies = []
    duration_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    duration_record_strategies.append(tv_testcase_util.RecordStrategyMin())
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(25))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(50))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(75))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(95))
    duration_record_strategies.append(tv_testcase_util.RecordStrategyMax())

    # Count recorders
    count_total_html_element_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "CntDomHtmlElements", count_record_strategies)
    count_document_html_element_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "CntDomDocumentHtmlElements",
        count_record_strategies)
    count_layout_box_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "CntLayoutBoxes", count_record_strategies)
    count_render_trees_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "CntRenderTrees", count_record_strategies)
    count_requested_images_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "CntRequestedImages", count_record_strategies)

    # Duration recorders
    duration_navigate_to_on_load_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "DurNavigateToOnLoadUs",
        duration_record_strategies)
    duration_navigate_to_browse_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "DurNavigateToBrowseUs",
        duration_record_strategies)
    duration_navigate_to_usable_recorder = tv_testcase_util.ResultsRecorder(
        STARTUP_RECORD_NAME + "DurNavigateToUsableUs",
        duration_record_strategies)

    # Now measure counts and durations from reloading the URL.
    for _ in range(NUM_LOAD_TV_ITERATIONS):
      count_render_trees_start = self.get_cval(
          c_val_names.count_rasterize_new_render_tree())

      self.load_tv()

      count_render_trees_end = self.get_cval(
          c_val_names.count_rasterize_new_render_tree())

      count_total_html_elements = self.get_cval(
          c_val_names.count_dom_html_elements_total())
      count_document_html_elements = self.get_cval(
          c_val_names.count_dom_html_elements_document())
      count_layout_boxes = self.get_cval(c_val_names.count_layout_boxes())
      count_requested_images = self.get_cval(
          c_val_names.count_image_cache_requested_resources())

      navigate_time = self.get_cval(c_val_names.time_browser_navigate())
      on_load_event_time = self.get_cval(
          c_val_names.time_browser_on_load_event())
      browse_time = self.get_cval(c_val_names.time_rasterize_new_render_tree())
      usable_time = self.get_cval(
          c_val_names.time_dom_html_script_element_execute())

      count_total_html_element_recorder.collect_value(count_total_html_elements)
      count_document_html_element_recorder.collect_value(
          count_document_html_elements)
      count_layout_box_recorder.collect_value(count_layout_boxes)
      count_render_trees_recorder.collect_value(count_render_trees_end -
                                                count_render_trees_start)
      count_requested_images_recorder.collect_value(count_requested_images)

      duration_navigate_to_on_load_recorder.collect_value(
          on_load_event_time - navigate_time)
      duration_navigate_to_browse_recorder.collect_value(
          browse_time - navigate_time)
      duration_navigate_to_usable_recorder.collect_value(
          usable_time - navigate_time)

    # Record the counts
    count_total_html_element_recorder.on_end_test()
    count_document_html_element_recorder.on_end_test()
    count_layout_box_recorder.on_end_test()
    count_render_trees_recorder.on_end_test()
    count_requested_images_recorder.on_end_test()

    # Record the durations
    tv_testcase_util.record_test_result(
        STARTUP_RECORD_NAME + "DurLaunchToNavigateUs",
        dur_launch_to_navigate_us)
    tv_testcase_util.record_test_result(
        STARTUP_RECORD_NAME + "DurLaunchToOnLoadUs",
        dur_launch_to_on_load_event_us)
    tv_testcase_util.record_test_result(
        STARTUP_RECORD_NAME + "DurLaunchToBrowseUs", dur_launch_to_browse_us)
    tv_testcase_util.record_test_result(
        STARTUP_RECORD_NAME + "DurLaunchToUsableUs", dur_launch_to_usable_us)

    duration_navigate_to_on_load_recorder.on_end_test()
    duration_navigate_to_browse_recorder.on_end_test()
    duration_navigate_to_usable_recorder.on_end_test()


if __name__ == "__main__":
  tv_testcase.main()
