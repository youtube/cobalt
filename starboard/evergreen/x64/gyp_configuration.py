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
"""Starboard Evergreen X64 platform configuration."""

import os.path

from starboard.evergreen.shared import gyp_configuration as shared_configuration
from starboard.tools import paths
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch


class EvergreenX64Configuration(shared_configuration.EvergreenConfiguration):
  """Starboard Evergreen X64 platform configuration."""

  def __init__(self,
               platform_name='evergreen-x64',
               asan_enabled_by_default=False,
               goma_supports_compiler=True):
    # pylint: disable=useless-super-delegation
    super(EvergreenX64Configuration,
          self).__init__(platform_name, asan_enabled_by_default,
                         goma_supports_compiler)

  def GetTargetToolchain(self):
    return self.GetHostToolchain()

  def GetHostToolchain(self):
    environment_variables = self.GetEnvironmentVariables()
    cc_path = environment_variables['CC']
    cxx_path = environment_variables['CXX']

    return [
        clang.CCompiler(path=cc_path),
        clang.CxxCompiler(path=cxx_path),
        clang.AssemblerWithCPreprocessor(path=cc_path),
        ar.StaticThinLinker(),
        ar.StaticLinker(),
        clangxx.ExecutableLinker(path=cxx_path),
        clangxx.SharedLibraryLinker(path=cxx_path),
        cp.Copy(),
        touch.Stamp(),
        bash.Shell(),
    ]

  def GetTestFilters(self):
    filters = super(EvergreenX64Configuration, self).GetTestFilters()
    # Remove the exclusion filter on SbDrmTest.AnySupportedKeySystems.
    # Generally, children of linux/shared do not support widevine, but children
    # of linux/x64x11 do, if the content decryption module is present.

    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'cdm', 'cdm',
                     'include', 'content_decryption_module.h'))

    if not has_cdm:
      return filters

    for test_filter in filters:
      if (test_filter.target_name == 'nplb' and
          test_filter.test_name == 'SbDrmTest.AnySupportedKeySystems'):
        filters.remove(test_filter)
    return filters


def CreatePlatformConfig():
  return EvergreenX64Configuration()
