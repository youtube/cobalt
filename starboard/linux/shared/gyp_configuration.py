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
"""Starboard Linux shared platform configuration."""

import os

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools import paths
from starboard.tools.testing import test_filter


class LinuxConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Linux platform configuration."""

  def __init__(self,
               platform,
               asan_enabled_by_default=True,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(LinuxConfiguration, self).__init__(platform, asan_enabled_by_default)

    self.sabi_json_path = sabi_json_path
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, config_name):
    variables = super(LinuxConfiguration, self).GetVariables(
        config_name, use_clang=1)
    variables.update({
        'include_path_platform_deploy_gypi':
            'starboard/linux/shared/platform_deploy.gypi',
    })
    return variables

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetGeneratorVariables(self, config_name):
    del config_name
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    if not hasattr(self, '_host_compiler_environment'):
      self._host_compiler_environment = build.GetHostCompilerEnvironment(
          clang.GetClangSpecification(), self.build_accelerator)

    env_variables = self._host_compiler_environment
    env_variables.update({
        'CC': self._host_compiler_environment['CC_host'],
        'CXX': self._host_compiler_environment['CXX_host'],
    })
    return env_variables

  def GetTestEnvVariables(self):
    # Due to fragile nature of dynamic TLS tracking, in particular LSAN reliance
    # on GLIBC private APIs, tracking TLS leaks is unstable and can trigger
    # sporadic false positives and/or crashes, depending on the used compiler,
    # direct library dependencies and runtime-loaded dynamic libraries. Hence,
    # TLS leak tracing in LSAN is turned off.
    # For reference, https://sourceware.org/ml/libc-alpha/2018-02/msg00567.html
    # When failing, 'LeakSanitizer has encountered a fatal error' message would
    # be printed at test shutdown, and env var LSAN_OPTIONS=verbosity=2 would
    # further point to 'Scanning DTLS range ..' prior to crash.
    return {'ASAN_OPTIONS': 'intercept_tls_get_addr=0'}

  def GetTestFilters(self):
    filters = super(LinuxConfiguration, self).GetTestFilters()

    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'ce_cdm', 'cdm',
                     'include', 'cdm.h'))

    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)

    if has_cdm:
      return filters

    # Filter the drm related tests, as ce_cdm is not present.
    for target, tests in self.__DRM_RELATED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetPathToSabiJsonFile(self):
    return self.sabi_json_path

  __DRM_RELATED_TESTS = {  # pylint: disable=invalid-name
      'nplb': [
          'SbDrmTest.AnySupportedKeySystems',
          'SbMediaCanPlayMimeAndKeySystem.AnySupportedKeySystems',
      ],
  }

  # pylint: disable=line-too-long
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'player_filter_tests': [
          # libdav1d crashes when fed invalid data
          'VideoDecoderTests/VideoDecoderTest.*Invalid*',
      ],
  }
  # Conditionally disables tests that require ipv6
  if os.getenv('IPV6_AVAILABLE', '1') == '0':
    __FILTERED_TESTS['nplb'] = [
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/1',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/1',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/1',
    ]

  # pylint: enable=line-too-long
