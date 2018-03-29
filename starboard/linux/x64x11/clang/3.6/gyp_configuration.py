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
"""Starboard Linux X64 X11 Clang 3.6 platform configuration."""

import logging
import os
import subprocess

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools import build


class LinuxX64X11Clang36Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux X64 X11 Clang 3.6 platform configuration."""

  def __init__(self, platform, asan_enabled_by_default=True):
    super(LinuxX64X11Clang36Configuration, self).__init__(
        platform, asan_enabled_by_default, goma_supports_compiler=False)

    script_path = os.path.dirname(os.path.realpath(__file__))
    # Run the script that ensures clang 3.6 is installed.
    subprocess.call(
        os.path.join(script_path, 'download_clang.sh'), cwd=script_path)
    self.toolchain_dir = os.path.join(build.GetToolchainsDir(),
                                      'x86_64-linux-gnu-clang-3.6', 'llvm',
                                      'Release+Asserts')

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
        'javascript_engine': 'mozjs-45',
        'cobalt_enable_jit': 0,
    })
    return variables


def CreatePlatformConfig():
  try:
    return LinuxX64X11Clang36Configuration('linux-x64x11-clang-3-6')
  except RuntimeError as e:
    logging.critical(e)
    return None
