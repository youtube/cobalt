# Copyright 2015 Google Inc. All Rights Reserved.
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
"""Starboard Linux shared platform configuration for gyp_cobalt."""

import os

from config.base import Configs
import config.starboard
import gyp_utils


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard Linux platform configuration."""

  def __init__(self, platform, asan_enabled_by_default=True):
    super(PlatformConfig, self).__init__(platform)

    gyp_utils.CheckClangVersion()

    # Check if goma is installed and initialize if needed.
    # There's no need to modify the compiler binary names if goma is set up
    # correctly in the PATH.
    gyp_utils.FindAndInitGoma()

    self.asan_default = 1 if asan_enabled_by_default else 0

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(configuration)
    use_tsan = int(os.environ.get('USE_TSAN', 0))
    # Enable ASAN by default for debug and devel builds only if USE_TSAN was not
    # set to 1 in the environment.
    use_asan_default = self.asan_default if not use_tsan and configuration in (
        Configs.DEBUG, Configs.DEVEL) else 0
    variables.update({
        'clang': 1,
        'use_asan': int(os.environ.get('USE_ASAN', use_asan_default)),
        'use_tsan': use_tsan,
    })

    if variables.get('use_asan') == 1 and variables.get('use_tsan') == 1:
      raise RuntimeError('ASAN and TSAN are mutually exclusive')
    return variables

  def GetGeneratorVariables(self, configuration):
    _ = configuration
    generator_variables = {'qtcreator_session_name_prefix': 'cobalt',}
    return generator_variables

  def GetEnvironmentVariables(self):
    env_variables = {
        'CC': 'clang',
        'CXX': 'clang++',
        'CC_host': 'clang',
        'CXX_host': 'clang++',
        'LD_host': 'clang++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }
    return env_variables
