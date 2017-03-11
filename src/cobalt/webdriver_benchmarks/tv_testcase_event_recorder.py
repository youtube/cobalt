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

    self.record_rasterize_animations = True
    self.record_video_start_delay = False
    self.skip_font_file_load_events = True


class EventRecorder(object):
  """Records stats on an event injected by a benchmark test.

  Handles collecting event data over all of the instances of a specific event
  injected by a benchmark test and records statistics on the data when the test
  ends. The specific event types that can be injected are listed within
  tv_testcase_util.py.

  Both rasterize animations and video start delay data can potentially be
  recorded, depending on the |record_rasterize_animations| and
  |record_video_start_delay| option flags.

  Additionally, events containing font file loads can be skipped if the
  |skip_font_file_load_events| option flag is set to True.
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
    self.skip_font_file_load_events = options.skip_font_file_load_events

    self.font_files_loaded_count = None

    self.render_tree_failure_count = 0
    self.font_file_load_skip_count = 0

    # Each entry in the list contains a tuple with a key and value recorder.
    self.value_dictionary_recorders = []

    self.animations_recorder = None
    self.video_delay_recorder = None

    # Count record strategies
    count_record_strategies = []
    count_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    count_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentile(50))

    # Count recorders
    self._add_value_dictionary_recorder("CntDomEventListeners",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomNodes", count_record_strategies)
    self._add_value_dictionary_recorder("CntDomHtmlElements",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomHtmlElementsCreated",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntDomHtmlElementsDestroyed",
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
    self._add_value_dictionary_recorder("CntLayoutBoxesDestroyed",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutUpdateSize",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutRenderAndAnimate",
                                        count_record_strategies)
    self._add_value_dictionary_recorder("CntLayoutUpdateCrossReferences",
                                        count_record_strategies)

    # Duration record strategies
    duration_record_strategies = []
    duration_record_strategies.append(tv_testcase_util.RecordStrategyMean())
    duration_record_strategies.append(
        tv_testcase_util.RecordStrategyPercentiles())

    # Duration recorders
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

    # Optional rasterize animations recorders
    if options.record_rasterize_animations:
      self.animations_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurRasterizeAnimationsUs",
          duration_record_strategies)

    # Optional video start delay recorder
    if options.record_video_start_delay:
      self.video_delay_recorder = tv_testcase_util.ResultsRecorder(
          self.event_name + "DurVideoStartDelayUs", duration_record_strategies)

  def _add_value_dictionary_recorder(self, key, record_strategies):
    recorder = tv_testcase_util.ResultsRecorder(self.event_name + key,
                                                record_strategies)
    self.value_dictionary_recorders.append((key, recorder))

  def on_start_event(self):
    """Handles logic related to the start of the event instance."""

    # If the recorder is set to skip events with font file loads and the font
    # file loaded count hasn't been initialized yet, then do that now.
    if self.skip_font_file_load_events and self.font_files_loaded_count is None:
      self.font_files_loaded_count = self.test.get_cval(
          c_val_names.count_font_files_loaded())

  def on_end_event(self):
    """Handles logic related to the end of the event instance."""

    # If the recorder is set to skip events with font file loads and a font file
    # loaded during the event, then its data is not collected. Log that it was
    # skipped and return.
    if self.skip_font_file_load_events:
      current_font_files_loaded_count = self.test.get_cval(
          c_val_names.count_font_files_loaded())
      if self.font_files_loaded_count != current_font_files_loaded_count:
        self.font_file_load_skip_count += 1
        print("{} event skipped because a font file loaded during it! {} "
              "events skipped.".format(self.event_name,
                                       self.font_file_load_skip_count))
        self.font_files_loaded_count = current_font_files_loaded_count
        return

    value_dictionary = self.test.get_cval(
        c_val_names.event_value_dictionary(self.event_type))

    # If the event failed to produce a render tree, then its data is not
    # collected. Log the failure and return.
    if not value_dictionary or not value_dictionary.get("ProducedRenderTree"):
      self.render_tree_failure_count += 1
      print("{} event failed to produce render tree! {} events failed.".format(
          self.event_name, self.render_tree_failure_count))
      return

    # Record all of the values from the event.
    for value_dictionary_recorder in self.value_dictionary_recorders:
      value = value_dictionary.get(value_dictionary_recorder[0])
      if value is not None:
        value_dictionary_recorder[1].collect_value(value)

    if self.animations_recorder:
      animation_entries = self.test.get_cval(
          c_val_names.rasterize_animations_entry_list())
      for value in animation_entries:
        self.animations_recorder.collect_value(value)

    if self.video_delay_recorder:
      self.video_delay_recorder.collect_value(
          self.test.get_cval(
              c_val_names.event_duration_dom_video_start_delay()))

  def on_end_test(self):
    """Handles logic related to the end of the test."""

    for value_dictionary_recorder in self.value_dictionary_recorders:
      value_dictionary_recorder[1].on_end_test()

    if self.animations_recorder:
      self.animations_recorder.on_end_test()

    if self.video_delay_recorder:
      self.video_delay_recorder.on_end_test()
