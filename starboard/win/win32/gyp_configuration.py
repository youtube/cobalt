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

import imp  # pylint: disable=deprecated-module
import logging
import os
import sys

import _env  # pylint: disable=unused-import
from starboard.shared.win32 import gyp_configuration

try:
  from gyp import MSVSVersion
except ImportError:
  # FIXME: hack to ensure that the MSVSVersion.py file is loaded when not run
  # as part of gyp invocation.
  _COBALT_SRC = os.path.abspath(os.path.join(*([__file__] + 4 * [os.pardir])))
  sys.path.append(os.path.join(_COBALT_SRC, 'tools', 'gyp', 'pylib'))
  from gyp import MSVSVersion


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

  def GetVariables(self, configuration):
    variables = super(WinWin32PlatformConfig, self).GetVariables(configuration)
    try:
      # TODO: Remove this conditional on docker builds.
      # Currently, only windows containers have consistent installed Redist/
      # folder versions. Builders on Trygerrit do not (yet) and this breaks
      # deployment of DLLs. The fix is to first standardize it for all build
      # environments, before removing this conditional.
      deploy_dlls = int(os.environ.get('BUILD_IN_DOCKER', 0))
    except (ValueError, TypeError):
      deploy_dlls = 0

    vs_version = MSVSVersion.SelectVisualStudioVersion(
        str(gyp_configuration.MSVS_VERSION))

    variables.update({
        # This controls whether the redistributable debug dlls are copied over
        # during build step.
        # This is important for being able to run binaries in within docker.
        'deploy_dlls': deploy_dlls,
        'visual_studio_toolset': vs_version.DefaultToolset()[1:],
    })
    return variables

  def GetTargetToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC']
    cxx_path = environment_variables['CXX']
    asm_path = environment_variables['ML']
    ar_path = environment_variables['AR']
    ld_path = environment_variables['LD']
    return self.GetToolsetToolchain(cc_path, cxx_path, asm_path, ar_path,
                                    ld_path, **kwargs)

  def GetHostToolchain(self, **kwargs):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC_HOST']
    cxx_path = environment_variables['CXX_HOST']
    asm_path = environment_variables['ML']
    ar_path = environment_variables['AR_HOST']
    ld_path = environment_variables['LD_HOST']
    return self.GetToolsetToolchain(cc_path, cxx_path, asm_path, ar_path,
                                    ld_path, **kwargs)
