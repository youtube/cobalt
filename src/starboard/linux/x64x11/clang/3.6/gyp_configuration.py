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
"""Starboard Linux X64 X11 Clang 3.6 platform configuration."""

import logging
import os
import subprocess

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools import build
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch


class LinuxX64X11Clang36Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux X64 X11 Clang 3.6 platform configuration."""

  def __init__(self,
               platform,
               asan_enabled_by_default=False,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(LinuxX64X11Clang36Configuration, self).__init__(
        platform,
        asan_enabled_by_default,
        goma_supports_compiler=False,
        sabi_json_path=sabi_json_path)

    self.toolchain_top_dir = os.path.join(build.GetToolchainsDir(),
                                          'x86_64-linux-gnu-clang-3.6')
    self.toolchain_dir = os.path.join(self.toolchain_top_dir, 'llvm',
                                      'Release+Asserts')

  def SetupPlatformTools(self, build_number):
    script_path = os.path.dirname(os.path.realpath(__file__))
    # Run the script that ensures clang 3.6 is installed.
    subprocess.call(
        os.path.join(script_path, 'download_clang.sh'), cwd=script_path)

  def GetEnvironmentVariables(self):
    env_variables = super(LinuxX64X11Clang36Configuration,
                          self).GetEnvironmentVariables()
    toolchain_bin_dir = os.path.join(self.toolchain_dir, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'clang'),
        'CXX': os.path.join(toolchain_bin_dir, 'clang++'),
    })
    return env_variables

  def GetVariables(self, config_name):
    # A significant amount of code in V8 fails to compile on clang 3.6 using
    # the debug config, due to an internal error in clang.
    variables = super(LinuxX64X11Clang36Configuration,
                      self).GetVariables(config_name)
    variables.update({
        'GCC_TOOLCHAIN_FOLDER':
            '\"%s\"' % os.path.join(self.toolchain_top_dir, 'libstdc++-7'),
    })
    return variables

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


def CreatePlatformConfig():
  try:
    return LinuxX64X11Clang36Configuration(
        'linux-x64x11-clang-3-6',
        sabi_json_path='starboard/sabi/x64/sysv/sabi-v{sb_api_version}.json')
  except RuntimeError as e:
    logging.critical(e)
    return None
