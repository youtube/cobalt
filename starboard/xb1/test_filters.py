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

import logging
import os

from starboard.shared.win32 import test_filters as shared_test_filters
from starboard.tools.testing import test_filter

# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        # Enable multiplayer tests once it's supported.
        'MultiplePlayerTests/*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.SecondaryPlayerTest/*',
    ],
    'player_filter_tests': [
        'PlayerComponentsTests/PlayerComponentsTest.*',
        'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/*',
        ('VideoDecoderTests/VideoDecoderTest'
         '.MultipleValidInputsAfterInvalidKeyFrame/*'),
        # AC3/EAC3 use a passthrough audio decoder that is incompatible with
        # AdaptiveAudioDecoderTests.
        # b/281740486.
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.MultipleInput/*c3*',
        'AdaptiveAudioDecoderTests/AdaptiveAudioDecoderTest.SingleInput/*c3*',
        'AudioDecoderTests/AudioDecoderTest.SingleInputHEAAC/*c3*',
    ],
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
    if os.environ.get('COBALT_WIN_BUILDBOT_DISABLE_TESTS', '0') == '1':
      logging.error('COBALT_WIN_BUILDBOT_DISABLE_TESTS=1, Tests are disabled.')
      return [test_filter.DISABLE_TESTING]

    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
