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
"""Starboard Raspberry Pi platform configuration."""

import logging
import os
import sys

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter


class RaspiPlatformConfig(platform_configuration.PlatformConfiguration):
  """Starboard Raspberry Pi platform configuration."""

  def __init__(self, platform):
    super(RaspiPlatformConfig, self).__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def _GetRasPiHome(self):
    try:
      raspi_home = os.environ['RASPI_HOME']
    except KeyError:
      logging.critical('RasPi builds require the "RASPI_HOME" '
                       'environment variable to be set.')
      sys.exit(1)
    return raspi_home

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    raspi_home = self._GetRasPiHome()
    sysroot = os.path.realpath(os.path.join(raspi_home, 'sysroot'))
    if not os.path.isdir(sysroot):
      logging.critical('RasPi builds require $RASPI_HOME/sysroot '
                       'to be a valid directory.')
      sys.exit(1)
    variables = super(RaspiPlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
        'sysroot': sysroot,
    })

    return variables

  def GetEnvironmentVariables(self):
    env_variables = build.GetHostCompilerEnvironment(
        clang.GetClangSpecification())
    raspi_home = self._GetRasPiHome()

    toolchain = os.path.realpath(
        os.path.join(
            raspi_home,
            'tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64'))
    toolchain_bin_dir = os.path.join(toolchain, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-g++'),
    })
    return env_variables

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
    for target, tests in self._FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  _FILTERED_TESTS = {
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
      'nplb_blitter_pixel_tests': [test_filter.FILTER_ALL],
      # TODO: enable player_filter_tests.
      'player_filter_tests': [test_filter.FILTER_ALL],
      'starboard_platform_tests': [test_filter.FILTER_ALL],
  }
