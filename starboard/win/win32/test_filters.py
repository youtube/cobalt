# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Starboard win-win32 Platform Test Filters."""

import logging
import os

from starboard.shared.win32 import test_filters as shared_test_filters
from starboard.tools.testing import test_filter


# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        # This single test takes >15 minutes.
        'SbPlayerTest.MultiPlayer',
        # This test fails on win-win32 devel builds, because the compiler
        # performs an optimization that defeats the SB_C_NOINLINE 'noinline'
        # attribute.
        'SbSystemGetStackTest.SunnyDayStackDirection',
    ],
    'player_filter_tests': [
        # These tests fail on our VMs for win-win32 builds due to missing
        # or non functioning system video decoders.
        'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/0',
        'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/2',
        'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/0',
        'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/2',
        'VideoDecoderTests/VideoDecoderTest.GetCurrentDecodeTargetBeforeWriteInputBuffer/0',
        'VideoDecoderTests/VideoDecoderTest.GetCurrentDecodeTargetBeforeWriteInputBuffer/2',
        'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/0',
        'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/2',
        'VideoDecoderTests/VideoDecoderTest.MaxNumberOfCachedFrames/0',
        'VideoDecoderTests/VideoDecoderTest.MaxNumberOfCachedFrames/2',
        'VideoDecoderTests/VideoDecoderTest.MultipleInputs/0',
        'VideoDecoderTests/VideoDecoderTest.MultipleInputs/2',
        'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/0',
        'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/2',
        'VideoDecoderTests/VideoDecoderTest.MultipleResets/0',
        'VideoDecoderTests/VideoDecoderTest.MultipleResets/2',
        'VideoDecoderTests/VideoDecoderTest.MultipleValidInputsAfterInvalidKeyFrame/0',
        'VideoDecoderTests/VideoDecoderTest.MultipleValidInputsAfterInvalidKeyFrame/2',
        'VideoDecoderTests/VideoDecoderTest.OutputModeSupported/0',
        'VideoDecoderTests/VideoDecoderTest.OutputModeSupported/2',
        'VideoDecoderTests/VideoDecoderTest.Preroll/0',
        'VideoDecoderTests/VideoDecoderTest.Preroll/2',
        'VideoDecoderTests/VideoDecoderTest.PrerollFrameCount/0',
        'VideoDecoderTests/VideoDecoderTest.PrerollFrameCount/2',
        'VideoDecoderTests/VideoDecoderTest.PrerollTimeout/0',
        'VideoDecoderTests/VideoDecoderTest.PrerollTimeout/2',
        'VideoDecoderTests/VideoDecoderTest.ResetAfterInput/0',
        'VideoDecoderTests/VideoDecoderTest.ResetAfterInput/2',
        'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/0',
        'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/2',
        'VideoDecoderTests/VideoDecoderTest.SingleInput/0',
        'VideoDecoderTests/VideoDecoderTest.SingleInput/2',
        'VideoDecoderTests/VideoDecoderTest.SingleInvalidKeyFrame/0',
        'VideoDecoderTests/VideoDecoderTest.SingleInvalidKeyFrame/2',
        'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/0',
        'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/1',
        'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/2',
        'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/3',

        # PlayerComponentsTests fail on our VMs. Preroll callback is always not called in
        # 5 seconds, which causes timeout error.
        'PlayerComponentsTests/*',
    ],
}

# pylint: enable=line-too-long


def CreateTestFilters():
  return WinWin32TestFilters()


class WinWin32TestFilters(shared_test_filters.TestFilters):
  """Starboard win-win32 Platform Test Filters."""

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    if os.environ.get('COBALT_WIN_BUILDBOT_DISABLE_TESTS', '0') == '1':
      logging.error('COBALT_WIN_BUILDBOT_DISABLE_TESTS=1, Tests are disabled.')
      return [test_filter.DISABLE_TESTING]
    else:
      filters = super(WinWin32TestFilters, self).GetTestFilters()
      for target, tests in _FILTERED_TESTS.iteritems():
        filters.extend(test_filter.TestFilter(target, test) for test in tests)
      if os.environ.get('EXPERIMENTAL_CI', '0') == '1':
        # Disable these tests in the experimental CI due to pending failures.
        experimental_filtered_tests = {
            'drain_file_test': [
                'DrainFileTest.SunnyDay',
                'DrainFileTest.SunnyDayPrepareDirectory',
                'DrainFileTest.RainyDayDrainFileAlreadyExists'
            ]
        }
        for target, tests in experimental_filtered_tests.iteritems():
          filters.extend(test_filter.TestFilter(target, test) for test in tests)
      return filters
