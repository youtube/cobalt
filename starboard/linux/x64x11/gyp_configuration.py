# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Linux X64 X11 platform configuration."""

import os.path

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch
from starboard.tools import paths


class LinuxX64X11Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux X64 X11 platform configuration."""

  def __init__(self,
               platform_name='linux-x64x11',
               asan_enabled_by_default=True,
               goma_supports_compiler=True,
               sabi_json_path=None):
    super(LinuxX64X11Configuration, self).__init__(
        platform_name,
        asan_enabled_by_default,
        goma_supports_compiler,
        sabi_json_path=sabi_json_path)

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
    filters = super(LinuxX64X11Configuration, self).GetTestFilters()
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
  return LinuxX64X11Configuration(sabi_json_path='starboard/sabi/x64/sabi.json')
