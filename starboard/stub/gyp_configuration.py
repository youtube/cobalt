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
"""Starboard stub platform configuration for gyp_cobalt."""

import logging

import config.starboard


def CreatePlatformConfig():
  try:
    return PlatformConfig('stub')
  except RuntimeError as e:
    logging.critical(e)
    return None


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard stub platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(configuration)
    variables.update({'clang': 1,})
    return variables

  def GetEnvironmentVariables(self):
    env_variables = {
        'CC': 'clang',
        'CXX': 'clang++',
        'CC_host': 'clang',
        'CXX_host': 'clang++',
        'LD_host': 'clang++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }
    return env_variables
