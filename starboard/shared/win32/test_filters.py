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
"""Starboard Win32 Platform Test Filters."""

from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    'nplb': [
        # Windows uses a special time zone format that ICU accepts, so we don't
        # enforce IANA.
        # TODO(b/304335954): Re-enable the test for UWP after fixing DST
        # implementation.
        'SbTimeZoneGetNameTest.IsIANAFormat',
    ],
}


class TestFilters(object):
  """Starboard Win32 platform test filters."""

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
