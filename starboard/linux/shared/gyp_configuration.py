# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Starboard Linux shared platform configuration."""

import os

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter


class LinuxConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Linux platform configuration."""

  def __init__(self,
               platform,
               asan_enabled_by_default=True,
               goma_supports_compiler=True):
    self.goma_supports_compiler = goma_supports_compiler
    super(LinuxConfiguration, self).__init__(platform, asan_enabled_by_default)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, config_name):
    variables = super(LinuxConfiguration, self).GetVariables(
        config_name, use_clang=1)
    variables.update({
        'javascript_engine': 'v8',
        'cobalt_enable_jit': 1,
    })
    return variables

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetGeneratorVariables(self, config_name):
    del config_name
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    if not hasattr(self, 'host_compiler_environment'):
      self.host_compiler_environment = build.GetHostCompilerEnvironment(
          clang.GetClangSpecification(), self.goma_supports_compiler)

    env_variables = self.host_compiler_environment
    env_variables.update({
        'CC': self.host_compiler_environment['CC_host'],
        'CXX': self.host_compiler_environment['CXX_host'],
    })
    return env_variables

  def GetTestFilters(self):
    filters = super(LinuxConfiguration, self).GetTestFilters()
    filters.extend([
        test_filter.TestFilter('starboard_platform_tests',
                               test_filter.FILTER_ALL),
    ])
    return filters
