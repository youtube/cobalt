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
"""Starboard Creator CI20 X11 gcc 4.9 platform configuration."""

import logging
import os

from starboard.contrib.creator.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


class PlatformConfig(shared_configuration.CreatorConfiguration):
  """Starboard Creator platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
    })
    toolchain_lib_path = os.path.join(self.toolchain_dir, 'lib')
    variables.update({
        'toolchain_lib_path': toolchain_lib_path,
    })
    return variables

  def GetEnvironmentVariables(self):
    env_variables = super(PlatformConfig, self).GetEnvironmentVariables()
    self.toolchain_dir = os.path.join(self.ci20_home)
    toolchain_bin_dir = os.path.join(self.toolchain_dir, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'mipsel-linux-gnu-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'mipsel-linux-gnu-g++'),
        'CC_host': 'gcc',
        'CXX_host': 'g++',
        'LD_host': 'g++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    })
    return env_variables

  def GetTestFilters(self):
    filters = super(PlatformConfig, self).GetTestFilters()
    filters.extend([
        # test fails when built with GCC 4.9, issue was fixed
        # in later versions of GCC
        test_filter.TestFilter('nplb', 'SbAlignTest.AlignAsStackVariable'),
        # tests fail also on x86
        test_filter.TestFilter('nplb', 'SbSystemSymbolizeTest.SunnyDay'),
        test_filter.TestFilter('nplb',
                               'SbSystemGetStackTest.SunnyDayStackDirection'),
        test_filter.TestFilter('nplb', 'SbSystemGetStackTest.SunnyDay'),
        test_filter.TestFilter('nplb',
                               'SbSystemGetStackTest.SunnyDayShortStack'),
    ])
    return filters


def CreatePlatformConfig():
  try:
    return PlatformConfig('creator-ci20x11-gcc-4-9')
  except RuntimeError as e:
    logging.critical(e)
    return None
