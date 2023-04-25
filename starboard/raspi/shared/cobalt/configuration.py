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
"""Starboard Raspberry Pi shared Cobalt configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltRaspiConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Raspberry Pi shared Cobalt configuration."""

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    filters.extend([
        # TODO: Remove this filter once the layout_tests slowdown in the debug
        # configuration is resolved.
        test_filter.TestFilter('layout_tests', test_filter.FILTER_ALL, 'debug'),
        # These tests are currently producing slightly different images on the
        # RasPi.
        test_filter.TestFilter('renderer_test',
                               'PixelTest.CircularSubPixelBorder'),
        test_filter.TestFilter('renderer_test',
                               'PixelTest.FilterBlurred100PxText'),
    ])
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
