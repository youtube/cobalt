# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Starboard Linux X64 DirectFB platform configuration for gyp_cobalt."""

import logging

# Import the shared Linux platform configuration.
from starboard.linux.shared import gyp_configuration
from starboard.tools.testing import test_filter


def CreatePlatformConfig():
  try:
    return PlatformConfig(
        'linux-x64directfb')
  except RuntimeError as e:
    logging.critical(e)
    return None


class PlatformConfig(gyp_configuration.PlatformConfig):

  # Unfortunately, some memory leaks outside of our control, and difficult
  # to pattern match with ASAN's suppression list, appear in DirectFB
  # builds, and so this must be disabled.
  def __init__(self, platform, asan_enabled_by_default=False,
               goma_supported_by_compiler=True):
    super(PlatformConfig, self).__init__(
        platform, asan_enabled_by_default, goma_supported_by_compiler)

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    return [
        test_filter.TestFilter(
            'bindings_test', ('GlobalInterfaceBindingsTest.'
                              'PropertiesAndOperationsAreOwnProperties')),
        test_filter.TestFilter(
            'net_unittests', 'HostResolverImplDnsTest.DnsTaskUnspec'),
        test_filter.TestFilter(
            'web_platform_tests', 'xhr/WebPlatformTest.Run/130', 'debug'),
        test_filter.TestFilter(
            'web_platform_tests', 'streams/WebPlatformTest.Run/11', 'debug'),
        test_filter.TestFilter(
            'starboard_platform_tests', test_filter.FILTER_ALL)
    ]
