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

        # Failures tracked by b/256160416.
        'SbSystemGetPathTest.ReturnsRequiredPaths',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.SeekAndDestroy/audio__null__video_beneath_the_canopy_137_avc_dmp_output_DecodeToTexture',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.NoInput/audio__null__video_beneath_the_canopy_137_avc_dmp_output_DecodeToTexture',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteSingleBatch/audio__null__video_beneath_the_canopy_137_avc_dmp_output_DecodeToTexture',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteMultipleBatches/audio__null__video_beneath_the_canopy_137_avc_dmp_output_DecodeToTexture',
        'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/type_ipv6_filter_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/type_ipv6',
        'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/filter_ipv6_type_ipv6',
    ],
    'player_filter_tests': [
        # These tests fail on our VMs for win-win32 builds due to missing
        # or non functioning system video decoders.
        'VideoDecoderTests/VideoDecoderTest.*/beneath_the_canopy_137_avc_dmp_DecodeToTexture*',
        'VideoDecoderTests/VideoDecoderTest.*/black_test_avc_1080p_30to60_fps_dmp_DecodeToTexture*',

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
      filters = super().GetTestFilters()
      for target, tests in _FILTERED_TESTS.items():
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
        for target, tests in experimental_filtered_tests.items():
          filters.extend(test_filter.TestFilter(target, test) for test in tests)
      return filters
