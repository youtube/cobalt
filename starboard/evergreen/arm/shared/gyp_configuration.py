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
"""Starboard Evergreen ARM shared platform configuration for gyp_cobalt."""

import os.path

from starboard.build import clang as clang_build
from starboard.evergreen.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import evergreen_linker
from starboard.tools.toolchain import touch


class EvergreenArmConfiguration(shared_configuration.EvergreenConfiguration):
  """Starboard Evergreen ARM platform configuration."""

  def __init__(self,
               platform='evergreen-arm',
               asan_enabled_by_default=False,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(EvergreenArmConfiguration, self).__init__(platform,
                                                    asan_enabled_by_default,
                                                    sabi_json_path)

    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetTargetToolchain(self, **kwargs):
    return self.GetHostToolchain(**kwargs)

  def GetHostToolchain(self, **kwargs):
    if not hasattr(self, '_host_toolchain'):
      env_variables = self.GetEnvironmentVariables()
      cc_path = env_variables['CC_host']
      cxx_path = env_variables['CXX_host']

      # Takes the provided value of CXX_HOST with a prepended 'ccache' and an
      # appended 'bin/clang++' and strips them off, leaving us with an absolute
      # path to the root directory of our toolchain.
      begin_path_index = cxx_path.find('/')
      end_path_index = cxx_path.rfind('/', 0, cxx_path.rfind('/')) + 1
      cxx_path_root = cxx_path[begin_path_index:end_path_index]

      self._host_toolchain = [
          clang.CCompiler(path=cc_path),
          clang.CxxCompiler(path=cxx_path),
          clang.AssemblerWithCPreprocessor(path=cc_path),
          ar.StaticThinLinker(),
          ar.StaticLinker(),
          clangxx.ExecutableLinker(path=cxx_path),
          evergreen_linker.SharedLibraryLinker(
              path=cxx_path_root, extra_flags=['-m armelf']),
          cp.Copy(),
          touch.Stamp(),
          bash.Shell(),
      ]
    return self._host_toolchain

  def GetTestFilters(self):
    filters = super(EvergreenArmConfiguration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetVariables(self, configuration):
    variables = super(EvergreenArmConfiguration,
                      self).GetVariables(configuration)
    variables.update({
        'include_path_platform_deploy_gypi':
            'starboard/evergreen/arm/shared/platform_deploy.gypi',
    })
    return variables

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'nplb': ['SbSystemGetStackTest.SunnyDayStackDirection',
               'SbSystemGetStackTest.SunnyDay',
               'SbSystemGetStackTest.SunnyDayShortStack',
               'SbSystemSymbolizeTest.SunnyDay'],
  }


def CreatePlatformConfig():
  return EvergreenArmConfiguration()
