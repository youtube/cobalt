# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Microsoft Xbox Cobalt configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltXB1Configuration(cobalt_configuration.CobaltConfiguration):
  """Starboard Microsoft Xbox Cobalt configuration."""

  def WebdriverBenchmarksEnabled(self):
    return True

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super().GetWebPlatformTestFilters()
    filters += [
        '*WebPlatformTest.Run*',
    ]
    return filters

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'base_unittests': ['PathServiceTest.Get',],
      # TODO(b/284008426): update comment or re-enable once fixed.
      'persistent_settings_test': [
          'PersistentSettingTest.DeleteSettings',
          'PersistentSettingTest.InvalidSettings'
      ],
      # TODO(b/275908073): update comment or re-enable once fixed.
      'renderer_test': ['StressTest.TooManyTextures'],
  }
