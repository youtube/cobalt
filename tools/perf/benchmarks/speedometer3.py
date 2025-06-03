# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Speedometer 3 Web Interaction Benchmark Pages
"""

import os
import re

from benchmarks import press

from core import path_util

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

from page_sets import speedometer3_pages

_PERF_TEST_DIR = os.path.join(path_util.GetChromiumSrcDir(), 'third_party',
                              'speedometer')


class _Speedometer3(press._PressBenchmark):  # pylint: disable=protected-access
  """Abstract base Speedometer3 Benchmark class.

  Runs all the speedometer 2 suites by default. Add --suite=<regex> to filter
  out suites, and only run suites whose names are matched by the regular
  expression provided.
  """

  enable_smoke_test_mode = False
  enable_systrace = False
  extra_chrome_categories = False
  enable_rcs = False
  iteration_count = None

  @classmethod
  def GetStoryClass(cls):
    raise NotImplementedError()

  def CreateStorySet(self, options):
    should_filter_suites = bool(options.suite)
    story_cls = self.GetStoryClass()
    filtered_suite_names = story_cls.GetSuites(options.suite)

    story_set = story.StorySet(base_dir=self._SOURCE_DIR)

    # For a smoke test one iteration is sufficient
    if self.enable_smoke_test_mode and not self.iteration_count:
      iteration_count = 1
    else:
      iteration_count = self.iteration_count

    story_set.AddStory(
        story_cls(story_set, should_filter_suites, filtered_suite_names,
                  iteration_count))
    return story_set

  def CreateCoreTimelinedMeasurementOptions(self):
    if not self.enable_systrace:
      return timeline_based_measurement.Options()

    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "toplevel" category is used to capture TaskQueueManager events.
    cat_filter.AddIncludedCategory('toplevel')

    if self.extra_chrome_categories:
      cat_filter.AddFilterString(self.extra_chrome_categories)

    if self.enable_rcs:
      # V8 needed categories
      cat_filter.AddIncludedCategory('v8')
      cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

      tbm_options = timeline_based_measurement.Options(
          overhead_level=cat_filter)
      tbm_options.SetTimelinedMetrics(['runtimeStatsTotalMetric'])
      return tbm_options

    tbm_options = timeline_based_measurement.Options(overhead_level=cat_filter)
    tbm_options.SetTimelinedMetrics(['tracingMetric'])
    return tbm_options

  def SetExtraBrowserOptions(self, options):
    if self.enable_rcs:
      options.AppendExtraBrowserArgs(
          '--enable-blink-features=BlinkRuntimeCallStats')

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--suite',
                      type="string",
                      help="Only runs suites that match regex provided")
    parser.add_option('--enable-rcs',
                      action="store_true",
                      help="Enables runtime call stats")
    parser.add_option('--iteration-count',
                      type="int",
                      help="Override the default number of iterations")

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if args.suite:
      try:
        if not cls.GetStoryClass().GetSuites(args.suite):
          raise parser.error('--suite: No matches.')
      except re.error:
        raise parser.error('--suite: Invalid regex.')
    if args.enable_systrace or args.enable_rcs:
      cls.enable_systrace = True
    if args.extra_chrome_categories:
      cls.extra_chrome_categories = args.extra_chrome_categories
    if args.enable_rcs:
      cls.enable_rcs = True
    if args.iteration_count:
      cls.iteration_count = args.iteration_count


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://browserbench.org/Speedometer3.0')
class Speedometer30(_Speedometer3):
  """Speedometer3.0 benchmark.
  Explicitly named version."""

  _SOURCE_DIR = os.path.join(_PERF_TEST_DIR, 'v3.0')

  @classmethod
  def GetStoryClass(cls):
    return speedometer3_pages.Speedometer30Story

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_speedometer3.0'


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://github.com/WebKit/Speedometer')
class Speedometer3(Speedometer30):
  """The latest version of the Speedometer3 benchmark."""
  @classmethod
  def GetStoryClass(cls):
    return speedometer3_pages.Speedometer3Story

  @classmethod
  def Name(cls):
    return 'speedometer3'


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://github.com/WebKit/Speedometer')
class V8Speedometer3Future(Speedometer3):
  """The latest Speedometer3 benchmark with the V8 flag --future.

  Shows the performance of upcoming V8 VM features.
  """
  @classmethod
  def Name(cls):
    return 'speedometer3-future'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')


@benchmark.Info(emails=['omerkatz@chromium.org'],
                component='Blink>JavaScript>GarbageCollection',
                documentation_url='https://github.com/WebKit/Speedometer')
class Speedometer3NoMinorMS(Speedometer3):
  """The latest Speedometer3 benchmark without the MinorMS flag.

  Shows the performance of Scavenger young generation GC in V8.
  """
  @classmethod
  def Name(cls):
    return 'speedometer3-nominorms'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--js-flags=--no-minor-ms')
