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
"""Starboard Darwin TvOS Platform Test Filters."""

from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    'nplb': [
        # UiNavigationTest expects the API to not be supported because the
        # web app is not exercising it yet. However, it is implemented on this
        # platform to allow development.
        'UiNavigationTest.GetInterface',

        # TODO(b/274023703): Enable partial audio tests once it's supported.
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.PartialAudio*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.DiscardAllAudio*',

        # Enable once verified on the platform.
        'SbMediaCanPlayMimeAndKeySystem.MinimumSupport',
    ],
    'base_unittests': [
        'StackTraceTest.OutputToStream',
        'StackTraceTest.TruncatedTrace',
        'StackTraceTest.TraceStackFramePointersFromBuffer',
    ],
}


class TestFilters(object):
  """Starboard Darwin TvOS platform test filters."""

  def GetTestFilters(self):
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
