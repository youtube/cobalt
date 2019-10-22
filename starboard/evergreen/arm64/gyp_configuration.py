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
"""Starboard evergreen-arm64 platform configuration for gyp_cobalt."""

from starboard.build import clang as clang_build
from starboard.evergreen.shared import gyp_configuration as shared_configuration
from starboard.tools import build
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import evergreen_linker
from starboard.tools.toolchain import touch


class EvergreenArm64Configuration(shared_configuration.EvergreenConfiguration):
  """Starboard Evergreen 64-bit ARM platform configuration."""

  def __init__(self,
               platform_name='evergreen-arm64',
               asan_enabled_by_default=False,
               goma_supports_compiler=True,
               sabi_json_path=None):
    # pylint: disable=useless-super-delegation
    super(EvergreenArm64Configuration,
          self).__init__(platform_name, asan_enabled_by_default,
                         goma_supports_compiler, sabi_json_path)
    self._host_toolchain = None

  def GetTargetToolchain(self):
    return self.GetHostToolchain()

  def GetHostToolchain(self):
    if not self._host_toolchain:
      if not hasattr(self, 'host_compiler_environment'):
        self.host_compiler_environment = build.GetHostCompilerEnvironment(
            clang_build.GetClangSpecification(), False)
      cc_path = self.host_compiler_environment['CC_host']
      cxx_path = self.host_compiler_environment['CXX_host']

      # Takes the provided value of CXX_HOST with a prepended 'gomacc' and an
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
              path=cxx_path_root, extra_flags=['-m aarch64elf']),
          cp.Copy(),
          touch.Stamp(),
          bash.Shell(),
      ]
    return self._host_toolchain

  def GetTestFilters(self):
    # pylint: disable=useless-super-delegation
    return super(EvergreenArm64Configuration, self).GetTestFilters()


def CreatePlatformConfig():
  return EvergreenArm64Configuration(
      sabi_json_path='starboard/evergreen/sabi/arm64/sabi.json')
