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
"""Starboard Evergreen shared platform configuration."""

import os

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools import paths
from starboard.tools.testing import test_filter


class EvergreenConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Evergreen platform configuration."""

  def __init__(self,
               platform,
               asan_enabled_by_default=True,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(EvergreenConfiguration, self).__init__(platform,
                                                 asan_enabled_by_default)

    self.sabi_json_path = sabi_json_path

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, config_name):
    variables = super(EvergreenConfiguration, self).GetVariables(
        config_name, use_clang=1)
    variables.update({
        'cobalt_repo_root':
            paths.REPOSITORY_ROOT,
        'include_path_platform_deploy_gypi':
            'starboard/evergreen/shared/platform_deploy.gypi',
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
    if not hasattr(self, '_host_compiler_environment'):
      self._host_compiler_environment = build.GetHostCompilerEnvironment(
          clang.GetClangSpecification(), self.build_accelerator)

    env_variables = self._host_compiler_environment
    env_variables.update({
        'CC': self._host_compiler_environment['CC_host'],
        'CXX': self._host_compiler_environment['CXX_host'],
    })
    return env_variables

  def GetPathToSabiJsonFile(self):
    return self.sabi_json_path

  def GetTestFilters(self):
    filters = super(EvergreenConfiguration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'nplb': ['MemoryReportingTest.CapturesOperatorDeleteNothrow',
               'SbAudioSinkTest.*',
               'SbDrmTest.AnySupportedKeySystems'],

      # player_filter_tests test the platform's Starboard implementation of
      # the filter-based player, which is not exposed through the Starboard
      # interface. Since Evergreen has no visibility of the platform's
      # specific Starboard implementation, rely on the platform to test this
      # directly instead.
      'player_filter_tests': [test_filter.FILTER_ALL],
  }

  def GetTestTargets(self):
    tests = super(EvergreenConfiguration, self).GetTestTargets()
    tests.append('cobalt_slot_management_test')
    return [test for test in tests if test not in self.__FORBIDDEN_TESTS]

  __FORBIDDEN_TESTS = [  # pylint: disable=invalid-name
      # elf_loader_test and installation_manager_test are explicitly tests that
      # validate the correctness of the underlying platform. We should not be
      # running these tests in Evergreen mode, and instead will rely on the
      # platform to test this directly instead.
      'elf_loader_test',
      'installation_manager_test',
  ]
