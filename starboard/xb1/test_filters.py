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
"""Starboard Xbox One Platform Test Filters."""

from starboard.shared.win32 import test_filters as shared_test_filters
from starboard.tools.testing import test_filter

# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        # Enable multiplayer tests once it's supported.
        'MultiplePlayerTests/*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.SecondaryPlayerTest/*',
        # TODO: b/333412348
        'SbMediaCanPlayMimeAndKeySystem.MinimumSupport',
        # TODO: b/333416764
        'PosixSocketHints/PosixSocketResolveTest.SunnyDayHints/family_inet6_*',
        'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/type_ipv6_filter_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/type_ipv6',
        'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/filter_ipv6_type_ipv6',
    ],
    'player_filter_tests': [
        'PlayerComponentsTests/PlayerComponentsTest.*',
        'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/*',
        ('VideoDecoderTests/VideoDecoderTest'
         '.MultipleValidInputsAfterInvalidKeyFrame/*'),
        'VideoDecoderTests/VideoDecoderTest.MultipleResets/sintel_399_av1_dmp_DecodeToTexture',
        # AC3/EAC3 use a passthrough audio decoder that is incompatible with
        # AdaptiveAudioDecoderTests.
        # b/281740486.
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.MultipleInput/*c3*',
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.SingleInput/*c3*',
        'AudioDecoderTests/AudioDecoderTest.SingleInputHEAAC/*c3*',
        # TODO: b/333412348
        'VideoDecoderTests/VideoDecoderTest.SingleInput/beneath_the_canopy_248_vp9_dmp_DecodeToTexture',
        'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/beneath_the_canopy_248_vp9_dmp_DecodeToTexture',
        'VideoDecoderTests/VideoDecoderTest.MultipleResets/beneath_the_canopy_248_vp9_dmp_DecodeToTexture',
    ],
    'trace_processor_minimal_smoke_tests': ['StorageMinimalSmokeTest/*',],
}


def CreateTestFilters():
  return Xb1TestFilters()


class Xb1TestFilters(shared_test_filters.TestFilters):
  """Starboard Xbox One Platform Test Filters."""

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
