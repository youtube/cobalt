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
"""Starboard Evergreen Platform Test Filters."""

from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    'nplb': [
        'MemoryReportingTest.CapturesOperatorDeleteNothrow',
        'SbAudioSinkTest.*', 'SbDrmTest.AnySupportedKeySystems'
    ],

    # player_filter_tests test the platform's Starboard implementation of
    # the filter-based player, which is not exposed through the Starboard
    # interface. Since Evergreen has no visibility of the platform's
    # specific Starboard implementation, rely on the platform to test this
    # directly instead.
    'player_filter_tests': [test_filter.FILTER_ALL],
}


class TestFilters(object):
  """Starboard Evergreen platform test filters."""

  def GetTestFilters(self):
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
