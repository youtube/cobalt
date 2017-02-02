#!/usr/bin/python2
"""Records stats on an event injected by a benchmark test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import c_val_names
import tv_testcase_c_val_recorder


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

  Handles collecting CVal event data over all of the instances of a specific
  event injected by a benchmark test and records statistics on the data when the
  test ends. The specific event types that can be injected are listed within
  tv_testcase_util.py.

  Both rasterize animations and video start delay data can potentially be
  recorded, depending on the |record_rasterize_animations| and
  |record_video_start_delay| option flags.

  Additionally, events containing font file loads can be skipped if the
  |skip_font_file_load_events| option flag is set to True.
  """

  def __init__(self, options):
    """Initializes the event recorder.

    Handles setting up this event recorder, including creating all of its CVal
    recorders, based upon the passed in options.

    Args:
      options: the settings to use in creating this event recorder
    """
    self.test = options.test
    self.event_name = options.event_name
    self.event_type = options.event_type
    self.skip_font_file_load_events = options.skip_font_file_load_events

    self.render_tree_failure_count = 0
    self.font_file_load_skip_count = 0

    self.c_val_recorders = []

    # Count cval recorders
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.count_dom_event_listeners(),
            self.event_name + "CntDomEventListeners"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.count_dom_nodes(),
            self.event_name + "CntDomNodes"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.count_dom_html_elements(),
            self.event_name + "CntDomHtmlElements"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_dom_html_elements_created(self.event_type),
            self.event_name + "CntDomHtmlElementsCreated"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_dom_html_elements_destroyed(
                self.event_type),
            self.event_name + "CntDomHtmlElementsDestroyed"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_dom_update_matching_rules(self.event_type),
            self.event_name + "CntDomUpdateMatchingRuleCalls"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_dom_update_computed_style(self.event_type),
            self.event_name + "CntDomUpdateComputedStyleCalls"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.count_layout_boxes(),
            self.event_name + "CntLayoutBoxes"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_layout_boxes_created(self.event_type),
            self.event_name + "CntLayoutBoxesCreated"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_count_layout_boxes_destroyed(self.event_type),
            self.event_name + "CntLayoutBoxesDestroyed"))

    # Duration cval recorders
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_total(self.event_type), self.event_name +
            "DurTotalUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_dom_inject_event(self.event_type),
            self.event_name + "DurDomInjectEventUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_dom_update_computed_style(
                self.event_type),
            self.event_name + "DurDomUpdateComputedStyleUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_layout_box_tree(self.event_type),
            self.event_name + "DurLayoutBoxTreeUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_layout_box_tree_box_generation(
                self.event_type), self.event_name +
            "DurLayoutBoxTreeBoxGenerationUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_layout_box_tree_update_used_sizes(
                self.event_type), self.event_name +
            "DurLayoutBoxTreeUpdateUsedSizesUs"))
    self.c_val_recorders.append(
        tv_testcase_c_val_recorder.FullCValRecorder(
            self.test,
            c_val_names.event_duration_layout_render_and_animate(
                self.event_type), self.event_name +
            "DurLayoutRenderAndAnimateUs"))

    # Optional rasterize animations cval recorders
    if options.record_rasterize_animations:
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.MeanCValRecorder(
              self.test,
              c_val_names.rasterize_animations_average(), self.event_name +
              "DurRasterizeAnimationsUs"))
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.PercentileCValRecorder(
              self.test,
              c_val_names.rasterize_animations_percentile_25(), self.event_name
              + "DurRasterizeAnimationsUs", 25))
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.PercentileCValRecorder(
              self.test,
              c_val_names.rasterize_animations_percentile_50(), self.event_name
              + "DurRasterizeAnimationsUs", 50))
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.PercentileCValRecorder(
              self.test,
              c_val_names.rasterize_animations_percentile_75(), self.event_name
              + "DurRasterizeAnimationsUs", 75))
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.PercentileCValRecorder(
              self.test,
              c_val_names.rasterize_animations_percentile_95(), self.event_name
              + "DurRasterizeAnimationsUs", 95))

    # Optional video start delay cval recorder
    if options.record_video_start_delay:
      self.c_val_recorders.append(
          tv_testcase_c_val_recorder.FullCValRecorder(
              self.test,
              c_val_names.event_duration_dom_video_start_delay(),
              self.event_name + "DurVideoStartDelayUs"))

  def on_start_event(self):
    """Handles logic related to the start of a new instance of the event."""

    # If the recorder is set to skip events with font file loads, then the font
    # files loaded count needs to be set prior to the event. That'll allow the
    # recorder to know whether or not font files were loaded during the event.
    if self.skip_font_file_load_events:
      self.start_event_font_files_loaded_count = self.test.get_int_cval(
          c_val_names.count_font_files_loaded())

  def on_end_event(self):
    """Handles logic related to the end of the event instance."""

    # If the event failed to produce a render tree, then its data is not
    # collected. Log the failure and return.
    if self.test.get_int_cval(
        c_val_names.event_produced_render_tree(self.event_type)) == 0:
      self.render_tree_failure_count += 1
      print("{} event failed to produce render tree! {} events failed.".format(
          self.event_name, self.render_tree_failure_count))
      return

    # If the event is set to skip events with font file loads and a font file
    # loaded during the event, then its data is not collected. Log that it was
    # skipped and return.
    if (self.skip_font_file_load_events and
        self.start_event_font_files_loaded_count !=
        self.test.get_int_cval(c_val_names.count_font_files_loaded())):
      self.font_file_load_skip_count += 1
      print("{} event skipped because a font file loaded during it! {} events "
            "skipped.".format(self.event_name, self.font_file_load_skip_count))
      return

    # Collect the current value of all of the CVal recorders.
    for c_val_recorder in self.c_val_recorders:
      c_val_recorder.collect_current_value()

  def on_end_test(self):
    """Handles logic related to the end of the test."""

    # Notify all of the CVal recorders that the test has ended.
    for c_val_recorder_entry in self.c_val_recorders:
      c_val_recorder_entry.on_end_test()
