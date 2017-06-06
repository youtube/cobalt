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
"""Starboard Android shared platform configuration for gyp_cobalt."""

from __future__ import print_function

import os

import config.starboard
import gyp_utils
import sdk_utils


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard Android platform configuration."""

  # TODO: make ASAN work with NDK tools and enable it by default
  def __init__(self, platform, android_abi, asan_enabled_by_default=False):
    super(PlatformConfig, self).__init__(platform, asan_enabled_by_default)

    self.android_abi = android_abi
    self.ndk_tools = sdk_utils.GetToolsPath(android_abi)
    sdk_utils.InstallSdkIfNeeded(android_abi)

    self.host_compiler_environment = gyp_utils.GetHostCompilerEnvironment()
    self.android_home = sdk_utils.GetSdkPath()
    self.android_ndk_home = sdk_utils.GetNdkPath()

    print('Using Android SDK at {}'.format(self.android_home))
    print('Using Android NDK at {}'.format(self.android_ndk_home))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(
        configuration, use_clang=1)
    variables.update({
        'ANDROID_HOME': self.android_home,
        'NDK_HOME': self.android_ndk_home,
        'NDK_SYSROOT': os.path.join(self.ndk_tools, 'sysroot'),
        'ANDROID_ABI': self.android_abi,
        'enable_remote_debugging': 0,
        'include_path_platform_deploy_gypi': 'starboard/android/shared/platform_deploy.gypi',
    })
    return variables

  def GetGeneratorVariables(self, configuration):
    _ = configuration
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    env_variables = sdk_utils.GetEnvironmentVariables(self.android_abi)
    env_variables.update(self.host_compiler_environment)
    return env_variables
