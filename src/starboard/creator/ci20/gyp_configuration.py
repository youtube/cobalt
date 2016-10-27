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
"""Starboard ci20 platform configuration for gyp_cobalt."""

import logging
import os
import sys

import config.starboard


def CreatePlatformConfig():
  try:
    return _PlatformConfig('creator-ci20')
  except RuntimeError as e:
    logging.critical(e)
    return None


class _PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard ci20 platform configuration."""

  def __init__(self, platform):
    super(_PlatformConfig, self).__init__(platform)

  def _GetCi20Home(self):
    try:
      ci20_home = os.environ['CI20_HOME']
    except KeyError:
      logging.critical('ci20 builds require the `CI20_HOME\' '
                       'environment variable to be set.')
      sys.exit(1)
    return ci20_home

  def GetVariables(self, configuration):
    ci20_home = self._GetCi20Home()

    sysroot = os.path.join(ci20_home, 'mips-mti-linux-gnu', '2016.05-03',
                           'sysroot')

    if not os.path.isdir(sysroot):
      logging.critical(
          'ci20 builds require '
          '$CI20_HOME/mips-mti-linux-gnu/2016.05-03/sysroot to be a valid '
          'directory.')
      sys.exit(1)
    variables = super(_PlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
        'sysroot': sysroot,
    })

    return variables

  def GetEnvironmentVariables(self):
    ci20_home = self._GetCi20Home()

    toolchain_bin_dir = os.path.join(ci20_home, 'mips-mti-linux-gnu',
                                     '2016.05-03', 'bin')

    env_variables = {
        'CC': os.path.join(toolchain_bin_dir, 'mips-mti-linux-gnu-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'mips-mti-linux-gnu-g++'),
        'CC_host': 'gcc',
        'CXX_host': 'g++',
        'LD_host': 'g++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }
    return env_variables
