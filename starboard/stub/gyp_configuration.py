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
"""Starboard stub platform configuration for gyp_cobalt."""

import logging

import config.base
from starboard.build import clang as clang_specification
from starboard.tools import build
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch

_SABI_JSON_PATH = 'starboard/sabi/x64/sysv/sabi-v{sb_api_version}.json'


def CreatePlatformConfig():
  try:
    return StubConfiguration('stub')
  except RuntimeError as e:
    logging.critical(e)
    return None


class StubConfiguration(config.base.PlatformConfigBase):
  """Starboard stub platform configuration."""

  def GetVariables(self, configuration):
    variables = super(StubConfiguration, self).GetVariables(
        configuration, use_clang=1)
    return variables

  def GetEnvironmentVariables(self):
    if not hasattr(self, 'host_compiler_environment'):
      self.host_compiler_environment = build.GetHostCompilerEnvironment(
          clang_specification.GetClangSpecification(), 'ccache')

    env_variables = self.host_compiler_environment
    env_variables.update({
        'CC': self.host_compiler_environment['CC_host'],
        'CXX': self.host_compiler_environment['CXX_host'],
    })
    return env_variables

  def GetTargetToolchain(self, **kwargs):  # pylint: disable=unused-argument
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

  def GetHostToolchain(self, **kwargs):  # pylint: disable=unused-argument
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

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    return []

  def GetPathToSabiJsonFile(self):
    return _SABI_JSON_PATH
