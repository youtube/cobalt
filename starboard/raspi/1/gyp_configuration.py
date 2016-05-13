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
"""Starboard Raspberry Pi 1 platform configuration for gyp_cobalt."""

import logging
import os
import sys

import config.starboard


def CreatePlatformConfig():
  try:
    return _PlatformConfig('raspi-1')
  except RuntimeError as e:
    logging.critical(e)
    return None


class _PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard Raspberry Pi 1 platform configuration."""

  def __init__(self, platform):
    super(_PlatformConfig, self).__init__(platform)

  def _GetRasPiHome(self):
    try:
      raspi_home = os.environ['RASPI_HOME']
    except KeyError:
      logging.critical('RasPi builds require the "RASPI_HOME" '
                       'environment variable to be set.')
      sys.exit(1)
    return raspi_home

  def GetVariables(self, configuration):
    raspi_home = self._GetRasPiHome()
    sysroot = os.path.realpath(os.path.join(raspi_home, 'sysroot'))
    if not os.path.isdir(sysroot):
      logging.critical('RasPi builds require $RASPI_HOME/sysroot '
                       'to be a valid directory.')
      sys.exit(1)
    variables = super(_PlatformConfig, self).GetVariables(configuration)
    variables.update({'clang': 0, 'sysroot': sysroot,})

    return variables

  def GetEnvironmentVariables(self):
    raspi_home = self._GetRasPiHome()

    toolchain = os.path.realpath(os.path.join(
        raspi_home,
        'tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64'))
    toolchain_bin_dir = os.path.join(toolchain, 'bin')
    env_variables = {
        'CC': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'arm-linux-gnueabihf-g++'),
        'CC_host': 'clang',
        'CXX_host': 'clang++',
        'LD_host': 'clang++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }
    return env_variables
