# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Linux X64 X11 dlmalloc platform configuration."""

from starboard.linux.x64x11 import gyp_configuration as linux_configuration


class LinuxX64X11DlmallocConfiguration(
    linux_configuration.LinuxX64X11Configuration):
  """Starboard Linux X64 X11 dlmalloc platform configuration."""

  def GetVariables(self, config_name):
    variables = super(LinuxX64X11DlmallocConfiguration,
                      self).GetVariables(config_name)
    variables.update({
        'use_asan': 0,
    })
    return variables


def CreatePlatformConfig():
  return LinuxX64X11DlmallocConfiguration(
      'linux-x64x11-dlmalloc',
      sabi_json_path='starboard/evergreen/sabi/x64/sabi.json')
