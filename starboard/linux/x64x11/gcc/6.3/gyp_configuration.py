# Copyright 2016 Google Inc. All Rights Reserved.
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
import subprocess

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools import build


class LinuxX64X11Gcc63Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux platform configuration."""

  def __init__(self, platform, asan_enabled_by_default=False):
    super(LinuxX64X11Gcc63Configuration, self).__init__(
        platform, asan_enabled_by_default, goma_supports_compiler=False)

    # Run the script that ensures gcc 6.3.0 is installed.
    script_path = os.path.dirname(os.path.realpath(__file__))
    subprocess.call(
        os.path.join(script_path, 'download_gcc.sh'), cwd=script_path)
    self.toolchain_dir = os.path.join(build.GetToolchainsDir(),
                                      'x86_64-linux-gnu-gcc-6.3.0', 'gcc')

  def GetVariables(self, configuration):
    variables = super(LinuxX64X11Gcc63Configuration,
                      self).GetVariables(configuration)
    variables.update({
        'clang': 0,
    })
    toolchain_lib_path = os.path.join(self.toolchain_dir, 'lib64')
    variables.update({
        'toolchain_lib_path': toolchain_lib_path,
    })
    return variables

  def GetEnvironmentVariables(self):
    env_variables = super(LinuxX64X11Gcc63Configuration,
                          self).GetEnvironmentVariables()
    toolchain_bin_dir = os.path.join(self.toolchain_dir, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'g++'),
    })
    return env_variables


def CreatePlatformConfig():
  return LinuxX64X11Gcc63Configuration('linux-x64x11-gcc-6-3')
