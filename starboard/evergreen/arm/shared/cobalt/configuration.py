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

import os

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltARMConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Cobalt Evergreen ARM configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltARMConfiguration,
          self).__init__(platform_configuration, application_name,
                         application_directory)

  def GetPostIncludes(self):
    # If there isn't a configuration.gypi found in the usual place, we'll
    # supplement with our shared implementation.
    includes = super(CobaltARMConfiguration, self).GetPostIncludes()
    for include in includes:
      if os.path.basename(include) == 'configuration.gypi':
        return includes

    shared_gypi_path = os.path.join(
        os.path.dirname(__file__), 'configuration.gypi')
    if os.path.isfile(shared_gypi_path):
      includes.append(shared_gypi_path)
    return includes

  def WebdriverBenchmarksEnabled(self):
    return True

  def GetTestFilters(self):
    filters = super(CobaltARMConfiguration, self).GetTestFilters()
    filters.extend([
        test_filter.TestFilter('bindings_test', 'DateBindingsTest.PosixEpoch'),
        # TODO: Remove this filter once the layout_tests slowdown in the debug
        # configuration is resolved.
        test_filter.TestFilter('layout_tests', test_filter.FILTER_ALL, 'debug'),
        test_filter.TestFilter('renderer_test',
                               'PixelTest.CircularSubPixelBorder'),
        test_filter.TestFilter('renderer_test',
                               'PixelTest.FilterBlurred100PxText'),
    ])
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super(CobaltARMConfiguration, self).GetWebPlatformTestFilters()
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
