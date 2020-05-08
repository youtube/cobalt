# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Starboard win-win32 platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp
import logging
import os
import sys

import _env  # pylint: disable=unused-import
import starboard.shared.win32.gyp_configuration as gyp_configuration
from starboard.tools.paths import STARBOARD_ROOT
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch
from starboard.tools.toolchain import msvc


def CreatePlatformConfig():
  try:
    win_lib_config = WinWin32PlatformConfig(
        'win-win32',
        sabi_json_path='starboard/sabi/x64/windows/sabi-v{sb_api_version}.json')
    return win_lib_config
  except RuntimeError as e:
    logging.critical(e)
    return None


class WinWin32PlatformConfig(gyp_configuration.Win32SharedConfiguration):
  """Starboard win-32 platform configuration."""

  def __init__(self,
               platform,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(WinWin32PlatformConfig, self).__init__(
        platform, sabi_json_path=sabi_json_path)

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'launcher.py'))

    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    if os.environ.get('COBALT_WIN_BUILDBOT_DISABLE_TESTS', '0') == '1':
      logging.error('COBALT_WIN_BUILDBOT_DISABLE_TESTS=1, Tests are disabled.')
      return [test_filter.DISABLE_TESTING]
    else:
      filters = super(WinWin32PlatformConfig, self).GetTestFilters()
      for target, tests in self.__FILTERED_TESTS.iteritems():
        filters.extend(test_filter.TestFilter(target, test) for test in tests)
      return filters

  __FILTERED_TESTS = {
      'nplb': [
          # This single test takes >15 minutes.
          'SbPlayerTest.MultiPlayer',
          # This test fails on win-win32 devel builds, because the compiler
          # performs an optimization that defeats the SB_C_NOINLINE 'noinline'
          # attribute.
          'SbSystemGetStackTest.SunnyDayStackDirection',
      ],
      'player_filter_tests': [
          # This test fails on win-win32.
          'AudioDecoderTests/AudioDecoderTest.MultipleInvalidInput/2',
          # These tests fail on our VMs for win-win32 builds due to missing
          # or non functioning system video decoders.
          'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/0',
          'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/2',
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/0',
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/2',
          'VideoDecoderTests/VideoDecoderTest.GetCurrentDecodeTargetBeforeWriteInputBuffer/0',
          'VideoDecoderTests/VideoDecoderTest.GetCurrentDecodeTargetBeforeWriteInputBuffer/2',
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/0',
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/2',
          'VideoDecoderTests/VideoDecoderTest.MaxNumberOfCachedFrames/0',
          'VideoDecoderTests/VideoDecoderTest.MaxNumberOfCachedFrames/2',
          'VideoDecoderTests/VideoDecoderTest.MultipleInputs/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleInputs/2',
          'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/2',
          'VideoDecoderTests/VideoDecoderTest.MultipleResets/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleResets/2',
          'VideoDecoderTests/VideoDecoderTest.MultipleValidInputsAfterInvalidKeyFrame/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleValidInputsAfterInvalidKeyFrame/2',
          'VideoDecoderTests/VideoDecoderTest.OutputModeSupported/0',
          'VideoDecoderTests/VideoDecoderTest.OutputModeSupported/2',
          'VideoDecoderTests/VideoDecoderTest.Preroll/0',
          'VideoDecoderTests/VideoDecoderTest.Preroll/2',
          'VideoDecoderTests/VideoDecoderTest.PrerollFrameCount/0',
          'VideoDecoderTests/VideoDecoderTest.PrerollFrameCount/2',
          'VideoDecoderTests/VideoDecoderTest.PrerollTimeout/0',
          'VideoDecoderTests/VideoDecoderTest.PrerollTimeout/2',
          'VideoDecoderTests/VideoDecoderTest.ResetAfterInput/0',
          'VideoDecoderTests/VideoDecoderTest.ResetAfterInput/2',
          'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/0',
          'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/2',
          'VideoDecoderTests/VideoDecoderTest.SingleInput/0',
          'VideoDecoderTests/VideoDecoderTest.SingleInput/2',
          'VideoDecoderTests/VideoDecoderTest.SingleInvalidKeyFrame/0',
          'VideoDecoderTests/VideoDecoderTest.SingleInvalidKeyFrame/2',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/0',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/1',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/2',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/3',
      ],
  }

  def GetVariables(self, configuration):
    variables = super(WinWin32PlatformConfig, self).GetVariables(configuration)
    # These variables will tell gyp to compile with V8.
    variables.update({
        'javascript_engine': 'v8',
    })
    return variables

  def GetTargetToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC']
    cxx_path = environment_variables['CXX']
    asm_path = 'ml.exe'
    ar_path = environment_variables['AR']
    ld_path = environment_variables['LD']
    return self.GetToolsetToolchain(cc_path, cxx_path, asm_path, ar_path,
                                    ld_path, **kwargs)

  def GetHostToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC_HOST']
    cxx_path = environment_variables['CXX_HOST']
    asm_path = 'ml.exe'
    ar_path = environment_variables['AR_HOST']
    ld_path = environment_variables['LD_HOST']
    return self.GetToolsetToolchain(cc_path, cxx_path, asm_path, ar_path,
                                    ld_path, **kwargs)

  def GetToolsetToolchain(self, cc_path, cxx_path, asm_path, ar_path, ld_path,
                          **kwargs):
    arch = 'environment.x64'
    gyp_defines = []
    gyp_include_dirs = []
    gyp_cflags = []
    gyp_cflags_cc = []
    gyp_libflags = []
    gyp_ldflags = []

    if 'spec' in kwargs and 'config_name' in kwargs:
      spec = kwargs['spec']
      config_name = kwargs['config_name']
      compiler_settings = self.GetCompilerSettings(
          spec,
          self.GetGeneratorVariables(None)
      )
      arch = 'environment.' + compiler_settings.GetArch(config_name)
      gyp_defines = gyp_defines + compiler_settings.GetDefines(config_name)
      gyp_include_dirs = compiler_settings.ProcessIncludeDirs(
          gyp_include_dirs,
          config_name
      )
      cflags = compiler_settings.GetCflags(config_name)
      cflags_c = compiler_settings.GetCflagsC(config_name)
      cflags_cc = compiler_settings.GetCflagsCC(config_name)
      gyp_cflags = gyp_cflags + cflags + cflags_c
      gyp_cflags_cc = gyp_cflags_cc + cflags + cflags_cc

      if 'gyp_path_to_ninja' in kwargs:
        gyp_path_to_ninja = kwargs['gyp_path_to_ninja']
        libflags = compiler_settings.GetLibFlags(config_name,
                                                 gyp_path_to_ninja)
        gyp_libflags = gyp_libflags + libflags

        keys = ['expand_special', 'gyp_path_to_unique_output',
                'compute_output_file_name']
        if all(k in kwargs for k in keys):
          expand_special = kwargs['expand_special']
          gyp_path_to_unique_output = kwargs['gyp_path_to_unique_output']
          compute_output_file_name = kwargs['compute_output_file_name']
          manifest_name = gyp_path_to_unique_output(
              compute_output_file_name(spec))
          is_executable = spec['type'] == 'executable'
          ldflags, manifest_files = compiler_settings.GetLdFlags(
              config_name,
              gyp_path_to_ninja=gyp_path_to_ninja,
              expand_special=expand_special,
              manifest_name=manifest_name,
              is_executable=is_executable
          )
          gyp_ldflags = gyp_ldflags + ldflags

    return [
        msvc.CCompiler(
            path=cc_path,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs,
            gyp_cflags=gyp_cflags
        ),
        msvc.CxxCompiler(
            path=cxx_path,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs,
            gyp_cflags=gyp_cflags_cc
        ),
        msvc.AssemblerWithCPreprocessor(
            path=asm_path,
            arch=arch,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs
        ),
        msvc.StaticThinLinker(
            path=ar_path,
            arch=arch,
            gyp_libflags=gyp_libflags
        ),
        msvc.StaticLinker(
            path=ar_path,
            arch=arch,
            gyp_libflags=gyp_libflags
        ),
        msvc.ExecutableLinker(
            path=ld_path,
            arch=arch,
            gyp_ldflags=gyp_ldflags
        ),
        msvc.SharedLibraryLinker(
            path=ld_path,
            arch=arch,
            gyp_ldflags=gyp_ldflags
        ),
        cp.Copy(),
        touch.Stamp(),
        bash.Shell(quote=False),
    ]

  def GetCompilerSettings(self, spec, generator_flags):
    sys.path.append(os.path.join(STARBOARD_ROOT, 'shared', 'msvc', 'uwp'))
    # pylint: disable=g-import-not-at-top,g-bad-import-order
    from msvc_toolchain import MsvsSettings
    return MsvsSettings(spec, generator_flags)
