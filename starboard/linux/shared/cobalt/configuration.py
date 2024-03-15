# Copyright 2017 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Starboard Linux Cobalt shared configuration."""

import os

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    # Tracked by b/185820828
    'net_unittests': ['SpdyNetworkTransactionTest.SpdyBasicAuth',],
    'base_unittests': [
        # Fails when run in a sharded configuration: b/233108722, b/216774170
        'TaskQueueSelectorTest.TestHighestPriority',
        'TaskQueueSelectorTest.TestHighPriority',
        'TaskQueueSelectorTest.TestLowPriority',
        'TaskQueueSelectorTest.TestBestEffortPriority',
        'TaskQueueSelectorTest.TestControlPriority',
        'TaskQueueSelectorTest.TestObserverWithEnabledQueue',
        'TaskQueueSelectorTest.TestObserverWithSetQueuePriorityAndQueueAlreadyEnabled',  #pylint: disable=line-too-long
        'TaskQueueSelectorTest.TestDisableEnable',
        'TaskQueueSelectorTest.TestDisableChangePriorityThenEnable',
        'TaskQueueSelectorTest.TestEmptyQueues',
        'TaskQueueSelectorTest.TestAge',
        'TaskQueueSelectorTest.TestControlStarvesOthers',
        'TaskQueueSelectorTest.TestHighestPriorityDoesNotStarveHigh',
        'TaskQueueSelectorTest.TestHighestPriorityDoesNotStarveHighOrNormal',
        'TaskQueueSelectorTest.TestHighestPriorityDoesNotStarveHighOrNormalOrLow',  #pylint: disable=line-too-long
        'TaskQueueSelectorTest.TestHighPriorityDoesNotStarveNormal',
        'TaskQueueSelectorTest.TestHighPriorityDoesNotStarveNormalOrLow',
        'TaskQueueSelectorTest.TestNormalPriorityDoesNotStarveLow',
        'TaskQueueSelectorTest.TestBestEffortGetsStarved',
        'TaskQueueSelectorTest.TestHighPriorityStarvationScoreIncreasedOnlyWhenTasksArePresent',  #pylint: disable=line-too-long
        'TaskQueueSelectorTest.TestNormalPriorityStarvationScoreIncreasedOnllWhenTasksArePresent',  #pylint: disable=line-too-long
        'TaskQueueSelectorTest.TestLowPriorityTaskStarvationOnlyIncreasedWhenTasksArePresent',  #pylint: disable=line-too-long
        'TaskQueueSelectorTest.AllEnabledWorkQueuesAreEmpty',
        'TaskQueueSelectorTest.AllEnabledWorkQueuesAreEmpty_ControlPriority',
        'TaskQueueSelectorTest.ChooseOldestWithPriority_Empty',
        'TaskQueueSelectorTest.ChooseOldestWithPriority_OnlyDelayed',
        'TaskQueueSelectorTest.ChooseOldestWithPriority_OnlyImmediate',
        'TaskQueueSelectorTest.TestObserverWithOneBlockedQueue',
        'TaskQueueSelectorTest.TestObserverWithTwoBlockedQueues',
        'HistogramTesterTest.GetHistogramSamplesSinceCreationNotNull',
        'HistogramTesterTest.TestUniqueSample',
        'HistogramTesterTest.TestBucketsSample',
        'HistogramTesterTest.TestBucketsSampleWithScope',
        'HistogramTesterTest.TestGetAllSamples',
        'HistogramTesterTest.TestGetAllSamples_NoSamples',
        'HistogramTesterTest.TestGetTotalCountsForPrefix',
        'HistogramTesterTest.TestGetAllChangedHistograms',

        # TODO: b/329269559 These have flaky ASAN heap-use-after-free
        # during metrics collection.
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue/WithMessagePump',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue/WithMessagePumpAlignedWakeUps',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue/WithMockTaskRunner',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue/WithMessagePump',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue/WithMessagePumpAlignedWakeUps',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue/WithMockTaskRunner',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.SweepCanceledDelayedTasks_ManyTasks/WithMessagePump',  # pylint: disable=line-too-long
        'All/SequenceManagerTest.SweepCanceledDelayedTasks_ManyTasks/WithMessagePumpAlignedWakeUps',  # pylint: disable=line-too-long
        'TaskEnvironmentTest.MultiThreadedMockTimeAndThreadPoolQueuedMode'
    ],
    'loader_test': [
        # TODO: b/329898292 These test are disabled because they fail.
        'FileFetcherTest.EmptyFile',
        'FileFetcherTest.ValidFile',
        'FileFetcherTest.ReadWithOffset',
        'FileFetcherTest.ReadWithOffsetAndSize',
        'LoaderTest.ValidFileEndToEndTest',

    ],
    'renderer_test': [
        # TODO: b/329899074 These test are disabled because they fail.
        'PixelTest.SimpleTextIn20PtFont',
        'PixelTest.SimpleTextIn40PtFont',
        'PixelTest.SimpleTextIn80PtFont',
        'PixelTest.SimpleTextIn500PtFont',
        'PixelTest.RedTextIn500PtFont',
        'PixelTest.TooManyGlyphs',
        'PixelTest.ShearedText',
        'PixelTest.SimpleText40PtFontWithCharacterLowerThanBaseline',
        'PixelTest.SimpleTextInEthiopic',
        'PixelTest.SimpleTextInEthiopicBold',
        'PixelTest.SimpleTextInRed40PtFont',
        'PixelTest.SimpleTextInRed40PtChineseFont',
        'PixelTest.RotatedTextInScaledRoundedCorners',
        'PixelTest.RedTextOnBlueIn40PtFont',
        'PixelTest.WhiteTextOnBlackIn40PtFont',
        'PixelTest.TransparentBlackTextOnRedIn40PtFont',
        'PixelTest.ViewportFilterOnTextNodeTest',
        'PixelTest.ViewportFilterWithTransformOnTextNodeTest',
        'PixelTest.ViewportFilterAndOpacityFilterOnTextNodeTest',
        'PixelTest.ScalingUpAnOpacityFilterTextDoesNotPixellate',
        'PixelTest.TextNodesScaleDownSmoothly',
        'PixelTest.DropShadowText',
        'PixelTest.DropShadowBlurred0Point1PxText',
        'PixelTest.DropShadowBlurred1PxText',
        'PixelTest.DropShadowBlurred2PxText',
        'PixelTest.DropShadowBlurred3PxText',
        'PixelTest.DropShadowBlurred4PxText',
        'PixelTest.DropShadowBlurred5PxText',
        'PixelTest.DropShadowBlurred6PxText',
        'PixelTest.DropShadowBlurred8PxText',
        'PixelTest.DropShadowBlurred20PxText',
        'PixelTest.ColoredDropShadowText',
        'PixelTest.ColoredDropShadowBlurredText',
        'PixelTest.MultipleColoredDropShadowText',
        'PixelTest.MultipleColoredDropShadowBlurredText',
        'PixelTest.FilterBlurred0Point1PxText',
        'PixelTest.FilterBlurred1PxText',
        'PixelTest.FilterBlurred2PxText',
        'PixelTest.FilterBlurred3PxText',
        'PixelTest.FilterBlurred4PxText',
        'PixelTest.FilterBlurred5PxText',
        'PixelTest.FilterBlurred6PxText',
        'PixelTest.FilterBlurred8PxText',
        'PixelTest.FilterBlurred20PxText',
    ],
}

if os.getenv('MODULAR_BUILD', '0') == '1':
  # Tracked by b/294070803
  _FILTERED_TESTS['layout_tests'] = [
      'CobaltSpecificLayoutTests/Layout.Test/platform_object_user_properties_survive_gc',  # pylint: disable=line-too-long
  ]
  # TODO: b/303260272 Re-enable these tests.
  _FILTERED_TESTS['blackbox'] = [
      'cancel_sync_loads_when_suspended',
      'preload_font',
      'preload_launch_parameter',
      'preload_visibility',
      'suspend_visibility',
  ]


class CobaltLinuxConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Linux Cobalt shared configuration."""

  def __init__(  # pylint:disable=useless-super-delegation
      self, platform_configuration, application_name, application_directory):
    super().__init__(platform_configuration, application_name,
                     application_directory)

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super().GetWebPlatformTestFilters()
    filters.extend([
        # These tests are timing-sensitive, and are thus flaky on slower builds
        test_filter.TestFilter(
            'web_platform_tests',
            'xhr/WebPlatformTest.Run/XMLHttpRequest_send_timeout_events_htm',
            'debug'),
        test_filter.TestFilter(
            'web_platform_tests',
            'streams/WebPlatformTest.Run/streams_readable_streams_templated_html',  # pylint:disable=line-too-long
            'debug'),
        test_filter.TestFilter(
            'web_platform_tests',
            'cors/WebPlatformTest.Run/cors_preflight_failure_htm', 'devel')
    ])
    return filters

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'crypto_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'net_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        # Tracked by b/294071365
        'starboard_platform_tests': {
            'ASAN_OPTIONS': 'detect_odr_violation=0'
        }
    }
