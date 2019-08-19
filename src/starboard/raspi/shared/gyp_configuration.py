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
"""Starboard Raspberry Pi platform configuration."""

import os

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter

# Use a bogus path instead of None so that anything based on $RASPI_HOME won't
# inadvertently end up pointing to something in the root directory, and this
# will show up in an error message when that fails.
_UNDEFINED_RASPI_HOME = '/UNDEFINED/RASPI_HOME'


class RaspiPlatformConfig(platform_configuration.PlatformConfiguration):
  """Starboard Raspberry Pi platform configuration."""

  def __init__(self, platform):
    super(RaspiPlatformConfig, self).__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))
    self.raspi_home = os.environ.get('RASPI_HOME', _UNDEFINED_RASPI_HOME)
    self.sysroot = os.path.realpath(os.path.join(self.raspi_home, 'sysroot'))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(RaspiPlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
        'sysroot': self.sysroot,
    })

    return variables

  def GetEnvironmentVariables(self):
    env_variables = build.GetHostCompilerEnvironment(
        clang.GetClangSpecification(), False)
    toolchain = os.path.realpath(
        os.path.join(
            self.raspi_home,
            'tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64'))
    toolchain_bin_dir = os.path.join(toolchain, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-g++'),
    })
    return env_variables

  def SetupPlatformTools(self, build_number):
    # Nothing to setup, but validate that RASPI_HOME is correct.
    if self.raspi_home == _UNDEFINED_RASPI_HOME:
      raise RuntimeError('RasPi builds require the "RASPI_HOME" '
                         'environment variable to be set.')
    if not os.path.isdir(self.sysroot):
      raise RuntimeError('RasPi builds require $RASPI_HOME/sysroot '
                         'to be a valid directory.')

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetGeneratorVariables(self, config_name):
    del config_name
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetTestFilters(self):
    filters = super(RaspiPlatformConfig, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  __FILTERED_TESTS = {
      'nplb': [
          'SbDrmTest.AnySupportedKeySystems',
          # The RasPi test devices don't have access to an IPV6 network, so
          # disable the related tests.
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDayDestination/1',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceForDestination/1',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceNotLoopback/1',
      ],
      'player_filter_tests': [
          # TODO: debug these failures.
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleResets/0',
          'VideoDecoderTests/VideoDecoderTest'
          '.MultipleValidInputsAfterInvalidKeyFrame/*',
          'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/*',
      ],
  }
