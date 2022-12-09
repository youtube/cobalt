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
"""Starboard Android Platform Test Filters."""

from starboard.tools.testing import test_filter

# A map of failing or crashing tests per target.
# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'player_filter_tests': [
        # Invalid input may lead to unexpected behaviors.
        'AudioDecoderTests/AudioDecoderTest.MultipleInvalidInput/*',
        'AudioDecoderTests/AudioDecoderTest.MultipleValidInputsAfterInvalidInput*',

        # GetMaxNumberOfCachedFrames() on Android is device dependent,
        # and Android doesn't provide an API to get it. So, this function
        # doesn't make sense on Android. But HoldFramesUntilFull tests depend
        # on this number strictly.
        'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/*',

        # Currently, invalid input tests are not supported.
        'VideoDecoderTests/VideoDecoderTest.SingleInvalidInput/*',
        ('VideoDecoderTests/VideoDecoderTest'
         '.MultipleValidInputsAfterInvalidKeyFrame/*'),
        'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/*',

        # Android currently does not support multi-video playback, which
        # the following tests depend upon.
        'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/*',

        # The video pipeline will hang if it doesn't receive any input.
        'PlayerComponentsTests/PlayerComponentsTest.EOSWithoutInput/*',

        # The e/eac3 audio time reporting during pause will be revisitied.
        'PlayerComponentsTests/PlayerComponentsTest.Pause/*ac3*',
        'PlayerComponentsTests/PlayerComponentsTest.Pause/*ec3*',
    ],
    'nplb': [
        # This test is failing because localhost is not defined for IPv6 in
        # /etc/hosts.
        'SbSocketAddressTypes/SbSocketResolveTest.Localhost/filter_ipv6_type_ipv6',

        # SbDirectory has problems with empty Asset dirs.
        'SbDirectoryCanOpenTest.SunnyDayStaticContent',
        'SbDirectoryGetNextTest.SunnyDayStaticContent',
        'SbDirectoryOpenTest.SunnyDayStaticContent',
        'SbFileGetPathInfoTest.WorksOnStaticContentDirectories',

        # These tests are disabled due to not receiving the kEndOfStream
        # player state update within the specified timeout.
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.NoInput/*',

        # Android does not use SbDrmSessionClosedFunc, which these tests
        # depend on.
        'SbDrmSessionTest.SunnyDay',
        'SbDrmSessionTest.CloseDrmSessionBeforeUpdateSession',

        # This test is failing because Android calls the
        # SbDrmSessionUpdateRequestFunc with SbDrmStatus::kSbDrmStatusSuccess
        # when invalid initialization data is passed to
        # SbDrmGenerateSessionUpdateRequest().
        'SbDrmSessionTest.InvalidSessionUpdateRequestParams',
    ],
}
# pylint: enable=line-too-long


class TestFilters(object):
  """Starboard Android platform test filters."""

  def GetTestFilters(self):
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
