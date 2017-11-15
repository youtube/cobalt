# Copyright 2016 Samsung Electronics. All Rights Reserved.
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
"""Starboard Tizen armv7l platform configuration for gyp_cobalt."""

import logging

import config.base


def CreatePlatformConfig():
  try:
    return _PlatformConfig('tizen-armv7l')
  except RuntimeError as e:
    logging.critical(e)
    return None


class _PlatformConfig(config.base.PlatformConfigBase):
  """Starboard Tizen Armv7l platform configuration."""

  def __init__(self, platform):
    super(_PlatformConfig, self).__init__(platform)

  def GetVariables(self, configuration):
    variables = super(_PlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
    })

    return variables

  def GetEnvironmentVariables(self):
    env_variables = {
        'CC': 'armv7l-tizen-linux-gnueabi-gcc',
        'CXX': 'armv7l-tizen-linux-gnueabi-g++',
        'CC_host': 'armv7l-tizen-linux-gnueabi-gcc',
        'CXX_host': 'armv7l-tizen-linux-gnueabi-g++',
    }
    return env_variables

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    return []
