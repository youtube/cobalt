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
"""Starboard Darwin Simulator Platform Test Filters."""

from internal.starboard.darwin.tvos.shared import test_filters as shared_test_filters
from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    # These tests sporadically fail likely due to timing and slow
    # performance with ASAN enabled.
    'nplb': [
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.SingleInput/*',
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.MultipleInput/*',
        'SbAudioSinkTest.AllFramesConsumed',
        'SbAudioSinkTest.SomeFramesConsumed',
        'SbSocketJoinMulticastGroupTest.SunnyDay',
        'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/type_ipv4_filter_ipv4',  # pylint: disable=line-too-long
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/type_ipv4',  # pylint: disable=line-too-long
        'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/filter_ipv4_type_ipv4',  # pylint: disable=line-too-long
        'Semaphore.ThreadTakesWait_TimeExpires',
    ],
    'player_filter_tests': [
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.MultipleInput/*',
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.SingleInput/*',
        'PlayerComponentsTests/PlayerComponentsTest.ShortPlayback/beneath_the_canopy_aac_stereo_dmp_beneath_the_canopy_137_avc_dmp_Punchout_0',  # pylint: disable=line-too-long
        'PlayerComponentsTests/PlayerComponentsTest.ShortPlayback/beneath_the_canopy_aac_mono_dmp_beneath_the_canopy_137_avc_dmp_Punchout_0',  # pylint: disable=line-too-long
        'PlayerComponentsTests/PlayerComponentsTest.ShortPlayback/beneath_the_canopy_opus_stereo_dmp_beneath_the_canopy_137_avc_dmp_Punchout_0',  # pylint: disable=line-too-long
        'PlayerComponentsTests/PlayerComponentsTest.ShortPlayback/beneath_the_canopy_opus_mono_dmp_beneath_the_canopy_137_avc_dmp_Punchout_0',  # pylint: disable=line-too-long
    ]
}


def CreateTestFilters():
  return DarwinTvOSSimulatorTestFilters()


class DarwinTvOSSimulatorTestFilters(shared_test_filters.TestFilters):
  """Starboard Darwin Simulator Platform Test Filters."""

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run."""

    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
