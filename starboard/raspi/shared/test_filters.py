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
"""Starboard Raspberry Pi Platform Test Filters."""

from starboard.tools.testing import test_filter

# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        'SbAudioSinkTest.*',

        # Permanently filter out drm system related tests as raspi doesn't
        # support any drm systems and there is no plan to implement such
        # support.
        'SbDrmTest.AnySupportedKeySystems',
        'SbMediaCanPlayMimeAndKeySystem.AnySupportedKeySystems',
        'SbMediaCanPlayMimeAndKeySystem.KeySystemWithAttributes',
        'SbMediaCanPlayMimeAndKeySystem.MinimumSupport',
        'SbMediaSetAudioWriteDurationTests/*',
        'SbPlayerWriteSampleTests*',
        'SbUndefinedBehaviorTest.CallThisPointerIsNullRainyDay',
        'SbSystemGetPropertyTest.FLAKY_ReturnsRequired',
    ],
    'player_filter_tests': [
        # The implementations for the raspberry pi (0 and 2) are incomplete
        # and not meant to be a reference implementation. As such we will
        # not repair these failing tests for now.
        'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/beneath_the_canopy_137_avc_dmp_Punchout',
        'VideoDecoderTests/VideoDecoderTest.MultipleResets/beneath_the_canopy_137_avc_dmp_Punchout',
        # Filter failed tests.
        'PlayerComponentsTests/PlayerComponentsTest.*',
    ],
}


class TestFilters(object):
  """Starboard Raspberry Pi platform test filters."""

  def GetTestFilters(self):
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters


def CreateTestFilters():
  return TestFilters()
