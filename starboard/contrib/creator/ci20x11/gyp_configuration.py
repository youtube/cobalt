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
"""Starboard Creator Ci20 X11 platform configuration."""

from starboard.contrib.creator.shared import gyp_configuration as shared_configuration
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch

class CreatorCI20X11Configuration(shared_configuration.CreatorConfiguration):
  """Starboard Creator X11 platform configuration."""

  def __init__(self, platform_name="creator-ci20x11"):
    super(CreatorCI20X11Configuration, self).__init__(platform_name)

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
        cp.Copy(),
        touch.Stamp(),
        bash.Shell(),
    ]

  def GetTestFilters(self):
    filters = super(CreatorCI20X11Configuration, self).GetTestFilters()
    return filters

def CreatePlatformConfig():
  return CreatorCI20X11Configuration()
