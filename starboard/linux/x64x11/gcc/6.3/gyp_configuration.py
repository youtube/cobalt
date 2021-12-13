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
"""Starboard Linux X64 X11 gcc 6.3 platform configuration for gyp_cobalt."""

import os

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch

# Directory for GCC 6.3 if it is installed as a system dependency.
_DEFAULT_GCC_6_3_BIN_DIR = '/usr/bin'
_DEFAULT_GCC_6_3_LIB_DIR = '/usr/lib/gcc/x86_64-linux-gnu/6'


class LinuxX64X11Gcc63Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux platform configuration."""

  def __init__(self,
               platform='linux-x64x11-gcc-6-3',
               asan_enabled_by_default=False,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(LinuxX64X11Gcc63Configuration,
          self).__init__(platform, asan_enabled_by_default, sabi_json_path)
    self.toolchain_bin_dir = _DEFAULT_GCC_6_3_BIN_DIR
    self.toolchain_lib_dir = _DEFAULT_GCC_6_3_LIB_DIR

  def SetupPlatformTools(self, build_number):  # pylint: disable=unused-argument
    if not (os.path.exists(os.path.join(self.toolchain_bin_dir, 'g++-6')) and
            os.path.exists(os.path.join(self.toolchain_bin_dir, 'gcc-6'))):
      raise RuntimeError('GCC 6.3 dependency is not present on this system.')

  def GetVariables(self, config_name):
    variables = super(LinuxX64X11Gcc63Configuration,
                      self).GetVariables(config_name)
    variables.update({
        'clang': 0,
    })
    variables.update({
        'toolchain_lib_path': self.toolchain_lib_dir,
    })
    return variables

  def GetEnvironmentVariables(self):
    env_variables = {
        'CC':
            self.build_accelerator + ' ' +
            os.path.join(self.toolchain_bin_dir, 'gcc-6'),
        'CXX':
            self.build_accelerator + ' ' +
            os.path.join(self.toolchain_bin_dir, 'g++-6'),
        'CC_HOST':
            self.build_accelerator + ' ' +
            os.path.join(self.toolchain_bin_dir, 'gcc-6'),
        'CXX_HOST':
            self.build_accelerator + ' ' +
            os.path.join(self.toolchain_bin_dir, 'g++-6'),
    }
    return env_variables

  def GetTargetToolchain(self, **kwargs):  # pylint: disable=unused-argument
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

  def GetHostToolchain(self, **kwargs):  # pylint: disable=unused-argument
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC_HOST']
    cxx_path = environment_variables['CXX_HOST']

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

  def GetTestFilters(self):
    filters = super(LinuxX64X11Gcc63Configuration, self).GetTestFilters()
    for target, tests in self._FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  _FILTERED_TESTS = {
      # TODO(b/206117361): Re-enable starboard_platform_tests once fixed.
      'starboard_platform_tests': ['JobQueueTest.JobsAreMovedAndNotCopied',],
  }


def CreatePlatformConfig():
  return LinuxX64X11Gcc63Configuration(
      sabi_json_path='starboard/sabi/x64/sysv/sabi-v{sb_api_version}.json')
