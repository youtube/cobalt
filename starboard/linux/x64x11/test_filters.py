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
"""Starboard Linux X64 X11 Platform Test Filters."""

from starboard.linux.shared import test_filters as shared_test_filters
from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    # TODO(b/321315344): Re-enable once the coverage build is fixed.
    'nplb': ['PosixMemoryMapTest.CanChangeMemoryProtection',],
}


def CreateTestFilters():
  return LinuxX64X11TestFilters()


class LinuxX64X11TestFilters(shared_test_filters.TestFilters):
  """Starboard Linux X64 X11 Platform Test Filters."""

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
