# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Cobalt Evergreen ARM configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltARMConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Cobalt Evergreen ARM configuration."""

  def __init__(  # pylint:disable=useless-super-delegation
      self, platform_configuration, application_name, application_directory):
    super().__init__(platform_configuration, application_name,
                     application_directory)

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super().GetWebPlatformTestFilters()
    filters += [
        ('csp/WebPlatformTest.Run/'
         'content_security_policy_media_src_media_src_allowed_html'),
        ('websockets/WebPlatformTest.Run/websockets_*'),
    ]
    return filters

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'crypto_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'net_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        }
    }

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
    'bindings_test': [
        'DateBindingsTest.PosixEpoch',
    ],
    'renderer_test': [
        'PixelTest.CircularSubPixelBorder',
        'PixelTest.FilterBlurred100PxText',
    ],
  }
