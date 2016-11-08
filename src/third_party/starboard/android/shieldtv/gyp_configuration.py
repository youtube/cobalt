# Copyright 2016 Google Inc. All Rights Reserved.
# Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
"""Starboard shieldtv platform configuration for gyp_cobalt."""

import os
import sys
import logging

import config.starboard


def CreatePlatformConfig():
  try:
    return PlatformConfig('android-shieldtv')
  except RuntimeError as e:
    logging.critical(e)
    return None


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard stub platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)

  def _GetNDKRoot(self):
    try:
      ndk_root = os.environ['NDK_ROOT']
    except KeyError:
      logging.critical('Android builds require the "NDK_ROOT" '
                       'environment variable to be set.')
      sys.exit(1)
    return ndk_root

  def GetVariables(self, configuration):
    ndk_root = self._GetNDKRoot()
    variables = super(PlatformConfig, self).GetVariables(configuration)
    variables.update({'clang': 0, 'ndk_root' : ndk_root})
    return variables

  def GetEnvironmentVariables(self):

    ndk_root = self._GetNDKRoot()

    toolchain = os.path.realpath(os.path.join(
        ndk_root, 'toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/'))

    env_variables = {
        'CC': os.path.join(toolchain, 'aarch64-linux-android-gcc'),
        'CXX': os.path.join(toolchain, 'aarch64-linux-android-g++'),
        'CC_host': 'clang',
        'CXX_host': 'clang++',
        'LD_host': 'clang++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }
    return env_variables
