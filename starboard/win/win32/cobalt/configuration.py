# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

"""Starboard Win32 Cobalt configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltWinWin32Configuration(cobalt_configuration.CobaltConfiguration):
  """Starboard Nintendo Switch Cobalt configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltWinWin32Configuration, self).__init__(
        platform_configuration, application_name, application_directory)
    self.vmware = platform_configuration.vmware

  def WebdriverBenchmarksEnabled(self):
    return False

  def GetTestFilters(self):
    filters = super(CobaltWinWin32Configuration, self).GetTestFilters()
    filtered_tests = dict(self.__FILTERED_TESTS)  # Copy.
    # On the VWware buildbot doesn't have a lot of video memory and
    # the following tests will fail or crash the system. Therefore they
    # are filtered out.
    # UPDATE: This might actually be a memory leak:
    #   https://b.***REMOVED***/issues/113123413
    # TODO: Remove these filters once the bug has been addressed.
    if self.vmware:
      filtered_tests.update({'layout_tests': [test_filter.FILTER_ALL]})
      filtered_tests.update({'renderer_test': [test_filter.FILTER_ALL]})

    for target, tests in filtered_tests.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super(CobaltWinWin32Configuration, self).GetWebPlatformTestFilters()
    filters += [
      '*WebPlatformTest.Run*',
    ]
    return filters

  __FILTERED_TESTS = {
      'renderer_test': [
          'ResourceProviderTest.ManyTexturesCanBeCreatedAndDestroyedQuickly', # Flaky.
          'ResourceProviderTest.TexturesCanBeCreatedFromSecondaryThread',
          'PixelTest.Width1Image',
          'PixelTest.Height1Image',
          'PixelTest.Area1Image',
      ],
  }
