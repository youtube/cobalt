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
"""Starboard Linux shared platform configuration for gyp_cobalt."""

import imp
import os

import config.base
import gyp_utils
from starboard.tools.testing import test_filter


class PlatformConfig(config.base.PlatformConfigBase):
  """Starboard Linux platform configuration."""

  def __init__(self,
               platform,
               asan_enabled_by_default=True,
               goma_supports_compiler=True):
    super(PlatformConfig, self).__init__(platform, asan_enabled_by_default)

    self.host_compiler_environment = gyp_utils.GetHostCompilerEnvironment(
        goma_supports_compiler)

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    return super(PlatformConfig, self).GetVariables(configuration, use_clang=1)

  def GetGeneratorVariables(self, configuration):
    _ = configuration
    generator_variables = {'qtcreator_session_name_prefix': 'cobalt',}
    return generator_variables

  def GetEnvironmentVariables(self):
    env_variables = self.host_compiler_environment
    env_variables.update({
        'CC': self.host_compiler_environment['CC_host'],
        'CXX': self.host_compiler_environment['CXX_host'],
    })
    return env_variables

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 'launcher.py'))
    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def WebdriverBenchmarksEnabled(self):
    return True

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
            'nplb_blitter_pixel_tests', test_filter.FILTER_ALL),
        test_filter.TestFilter(
            'web_platform_tests', 'xhr/WebPlatformTest.Run/130', 'debug'),
        test_filter.TestFilter(
            'web_platform_tests', 'streams/WebPlatformTest.Run/11', 'debug'),
        test_filter.TestFilter(
            'starboard_platform_tests', test_filter.FILTER_ALL)
    ]

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'crypto_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'net_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'}
    }
