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

from starboard.build import clang as clang_specification
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch

# Use a bogus path instead of None so that anything based on $RASPI_HOME won't
# inadvertently end up pointing to something in the root directory, and this
# will show up in an error message when that fails.
_UNDEFINED_RASPI_HOME = '/UNDEFINED/RASPI_HOME'


class RaspiPlatformConfig(platform_configuration.PlatformConfiguration):
  """Starboard Raspberry Pi platform configuration."""

  def __init__(self,
               platform,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(RaspiPlatformConfig, self).__init__(platform)

    self.sabi_json_path = sabi_json_path
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))
    self.raspi_home = os.environ.get('RASPI_HOME', _UNDEFINED_RASPI_HOME)
    self.sysroot = os.path.realpath(os.path.join(self.raspi_home, 'busterroot'))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(RaspiPlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang':
            0,
        'sysroot':
            self.sysroot,
        'include_path_platform_deploy_gypi':
            'starboard/raspi/shared/platform_deploy.gypi',
        'STRIP':
            os.environ.get('STRIP')
    })

    return variables

  def GetEnvironmentVariables(self):
    if not hasattr(self, 'host_compiler_environment'):
      self.host_compiler_environment = build.GetHostCompilerEnvironment(
          clang_specification.GetClangSpecification(), self.build_accelerator)

    toolchain = os.path.realpath(
        os.path.join(
            self.raspi_home, 'tools/arm-bcm2708/'
            'gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf'))
    toolchain_bin_dir = os.path.join(toolchain, 'bin')

    env_variables = self.host_compiler_environment
    env_variables.update({
        'CC':
            self.build_accelerator + ' ' +
            os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-gcc'),
        'CXX':
            self.build_accelerator + ' ' +
            os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-g++'),
        'STRIP':
            os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-strip'),
    })
    return env_variables

  def SetupPlatformTools(self, build_number):
    # Nothing to setup, but validate that RASPI_HOME is correct.
    if self.raspi_home == _UNDEFINED_RASPI_HOME:
      raise RuntimeError('RasPi builds require the "RASPI_HOME" '
                         'environment variable to be set.')
    if not os.path.isdir(self.sysroot):
      raise RuntimeError('RasPi builds require $RASPI_HOME/busterroot '
                         'to be a valid directory.')

  def GetTargetToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC']
    cxx_path = environment_variables['CXX']

    return [
        clang.CCompiler(path=cc_path),
        clang.CxxCompiler(path=cxx_path),
        clang.AssemblerWithCPreprocessor(path=cc_path),
        ar.StaticThinLinker(),
        ar.StaticLinker(),
        clangxx.ExecutableLinker(path=cxx_path, write_group=True),
        clangxx.SharedLibraryLinker(path=cxx_path),
        cp.Copy(),
        touch.Stamp(),
        bash.Shell(),
    ]

  def GetHostToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC_host']
    cxx_path = environment_variables['CXX_host']

    return [
        clang.CCompiler(path=cc_path),
        clang.CxxCompiler(path=cxx_path),
        clang.AssemblerWithCPreprocessor(path=cc_path),
        ar.StaticThinLinker(),
        ar.StaticLinker(),
        clangxx.ExecutableLinker(path=cxx_path, write_group=True),
        clangxx.SharedLibraryLinker(path=cxx_path),
        cp.Copy(),
        touch.Stamp(),
        bash.Shell(),
    ]

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

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'nplb': [
          'SbAudioSinkTest.*',
          'SbDrmTest.AnySupportedKeySystems',
          'SbMediaCanPlayMimeAndKeySystem.AnySupportedKeySystems',
          'SbMediaCanPlayMimeAndKeySystem.KeySystemWithAttributes',
          'SbMediaCanPlayMimeAndKeySystem.MinimumSupport',
          'SbMediaSetAudioWriteDurationTests/*',
          'SbPlayerWriteSampleTests*',
          'SbUndefinedBehaviorTest.CallThisPointerIsNullRainyDay',
          'SbSystemGetPropertyTest.FLAKY_ReturnsRequired',
      ],
      'player_filter_tests': [
          # The implementations for the raspberry pi (0 and 2) are incomplete
          # and not meant to be a reference implementation. As such we will
          # not repair these failing tests for now.
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleResets/0',
          # Filter failed tests.
          'PlayerComponentsTests/PlayerComponentsTest.*',

      ],
  }

  def GetPathToSabiJsonFile(self):
    return self.sabi_json_path
