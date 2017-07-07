#!/usr/bin/python2
"""Records stats on an event injected by a benchmark test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import c_val_names
import tv_testcase_util


class EventRecorderOptions(object):
  """Contains all of the parameters needed by EventRecorder."""

  def __init__(self, test, event_name, event_type):
    """Initializes the event recorder options.

    Args:
      test: specific benchmark test being run
      event_name: name of the event being recorded
      event_type: type of event being recorded (tv_testcase_util.py)
    """
    self.test = test
    self.event_name = event_name
    self.event_type = event_type

    self.record_animations = True
    self.record_video = False


class EventRecorder(object):
  """Records stats on an event injected by a benchmark test.

  Handles collecting event data over all of the instances of a specific event
  injected by a benchmark test and records statistics on the data when the test
  ends. The specific event types that can be injected are listed within
  tv_testcase_util.py.

  Both rasterize animations and video start delay data can potentially be
  recorded, depending on the |record_animations| and |record_video| option
  flags.
  """

  def __init__(self, options):
    """Initializes the event recorder.

    Handles setting up this event recorder, including creating all of its
    recorders, based upon the passed in options.

    Args:
      options: the settings to use in creating this event recorder
    """
    self.test = options.test
    self.event_name = options.event_name
    self.event_type = options.event_type

    self.render_tree_failure_count = 0

    # Each entry in the list contains a tuple with a key and value recorder.
    self.value_dictionary_recorders = []

    # Optional animations recorders
    self.animations_recorder = None
    self.animations_start_delay_recorder = None
    self.animations_end_delay_recorder = None

    # Optional video recorders
    self.video_delay_recorder = None

    # Count record strategies
    count_record_strategies = []
    count_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    count_record_strategies.append(tv_testcase_util.RecordStrategyMedian())
    count_record_strategies.append(tv_testcase_util.RecordStrategyMax())

    # Duration record strategies
    duration_record_strategies = []
    duration_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(25))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(50))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(75))
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(95))

    # Video delay record strategies
    video_delay_record_strategies = []
    video_delay_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    video_delay_record_strategies.append(tv_testcase_util.RecordStrategyMin())
    video_delay_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(25))
    video_delay_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(50))
    video_delay_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(75))
    video_delay_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(95))
    video_delay_record_strategies.append(tv_testcase_util.RecordStrategyMax())

    # Dictionary count recorders
    self._add_value_dictionary_recorder("CntDomEventListeners",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomNodes", count_record_strategies)
    self._add_value_dictionary_recorder("CntDomHtmlElements",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomDocumentHtmlElements",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomHtmlElementsCreated",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomUpdateMatchingRules",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomUpdateComputedStyle",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomGenerateHtmlComputedStyle",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomGeneratePseudoComputedStyle",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutBoxes",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutBoxesCreated",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutUpdateSize",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutRenderAndAnimate",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutUpdateCrossReferences",
                                        count_record_strategies)

    # Dictionary duration recorders
    self._add_value_dictionary_recorder("DurTotalUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurDomInjectEventUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurDomRunAnimationFrameCallbacksUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurDomUpdateComputedStyleUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurLayoutBoxTreeUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurLayoutBoxTreeBoxGenerationUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurLayoutBoxTreeUpdateUsedSizesUs",
                                        duration_record_strategies)
    self._add_value_dictionary_recorder("DurLayoutRenderAndAnimateUs",
                                        duration_record_strategies)

    self.count_render_trees_recorder = tv_testcase_util.ResultsRecorder(
        self.event_name + "CntRenderTrees", count_record_strategies)
    self.count_requested_images_recorder = tv_testcase_util.ResultsRecorder(
        self.event_name + "CntRequestedImages", count_record_strategies)

    # Optional animations recorder
    if options.record_animations:
      self.animations_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurRasterizeAnimationsUs",
          duration_record_strategies)
      self.animations_start_delay_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurAnimationsStartDelayUs",
          duration_record_strategies)
      self.animations_end_delay_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurAnimationsEndDelayUs",
          duration_record_strategies)

    self.final_render_tree_delay_recorder = tv_testcase_util.ResultsRecorder(
        self.event_name + "DurFinalRenderTreeDelayUs",
        duration_record_strategies)

    # Optional video recorder
    if options.record_video:
      self.video_delay_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurVideoStartDelayUs",
          video_delay_record_strategies)

  def _add_value_dictionary_recorder(self, key, record_strategies):
    recorder = tv_testcase_util.ResultsRecorder(self.event_name + key,
                                                record_strategies)
    self.value_dictionary_recorders.append((key, recorder))

  def on_start_event(self):
    """Handles logic related to the start of the event instance."""
    self.count_render_trees_start = self.test.get_cval(
        c_val_names.count_rasterize_new_render_tree())
    self.count_requested_images_start = self.test.get_cval(
        c_val_names.count_image_cache_requested_resources())

  def on_end_event(self):
    """Handles logic related to the end of the event instance."""

    value_dictionary = self.test.get_cval(
        c_val_names.event_value_dictionary(self.event_type))

    # If the event failed to produce a render tree, then its data is not
    # collected. Log the failure and return.
    if not value_dictionary or not value_dictionary.get("ProducedRenderTree"):
      self.render_tree_failure_count += 1
      print("{} event failed to produce render tree! {} events failed.".format(
          self.event_name, self.render_tree_failure_count))
      return

    event_start_time = value_dictionary.get("StartTime")

    # Record all of the values from the event.
    for value_dictionary_recorder in self.value_dictionary_recorders:
      value = value_dictionary.get(value_dictionary_recorder[0])
      if value is not None:
        value_dictionary_recorder[1].collect_value(value)

    self.count_render_trees_end = self.test.get_cval(
        c_val_names.count_rasterize_new_render_tree())
    self.count_render_trees_recorder.collect_value(
        self.count_render_trees_end - self.count_render_trees_start)

    self.count_requested_images_end = self.test.get_cval(
        c_val_names.count_image_cache_requested_resources())
    self.count_requested_images_recorder.collect_value(
        self.count_requested_images_end - self.count_requested_images_start)

    if self.animations_recorder:
      animation_entries = self.test.get_cval(
          c_val_names.rasterize_animations_entry_list())
      for value in animation_entries:
        self.animations_recorder.collect_value(value)

      animations_start_time = self.test.get_cval(
          c_val_names.time_rasterize_animations_start())
      self.animations_start_delay_recorder.collect_value(
          animations_start_time - event_start_time)
      animations_end_time = self.test.get_cval(
          c_val_names.time_rasterize_animations_end())
      self.animations_end_delay_recorder.collect_value(animations_end_time -
                                                       event_start_time)

    final_render_tree_time = self.test.get_cval(
        c_val_names.time_rasterize_new_render_tree())
    self.final_render_tree_delay_recorder.collect_value(final_render_tree_time -
                                                        event_start_time)

    if self.video_delay_recorder:
      self.video_delay_recorder.collect_value(
          self.test.get_cval(
              c_val_names.event_duration_dom_video_start_delay()))

  def on_end_test(self):
    """Handles logic related to the end of the test."""

    for value_dictionary_recorder in self.value_dictionary_recorders:
      value_dictionary_recorder[1].on_end_test()

    self.count_render_trees_recorder.on_end_test()
    self.count_requested_images_recorder.on_end_test()

    if self.animations_recorder:
      self.animations_recorder.on_end_test()
      self.animations_start_delay_recorder.on_end_test()
      self.animations_end_delay_recorder.on_end_test()

    self.final_render_tree_delay_recorder.on_end_test()

    if self.video_delay_recorder:
      self.video_delay_recorder.on_end_test()
