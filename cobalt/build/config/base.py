#!/usr/bin/python
# Copyright 2014 Google Inc. All Rights Reserved.
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
"""Base platform configuration for GYP."""

import imp
import importlib
import logging
import os

import gyp_utils

# Represents all valid build configurations.
VALID_BUILD_CONFIGS = ['debug', 'devel', 'qa', 'gold']

# Represents all supported platforms, uniquified and sorted.
VALID_PLATFORMS = sorted(list(set(['linux', 'ps3', 'win'] +
                                  gyp_utils.GetThirdPartyPlatforms().keys())))

_CURRENT_PATH = os.path.abspath(os.path.dirname(__file__))


class PlatformConfigBase(object):
  """Base platform configuration class.

  Should be derived by platform specific configurations.
  """

  def __init__(self, platform):
    self.platform = platform
    self.config_path = _CURRENT_PATH

  def IsStarboard(self):
    """Returns whether this platform is a Starboard platform."""
    return False

  def GetBuildFormat(self):
    """Returns the desired build format."""
    return 'ninja'

  def GetIncludes(self):
    """Returns a list of gypi files.

    These files will be included by GYP before any processed .gyp file.

    Returns:
        A list containing paths to .gypi files.
    """
    platforms = gyp_utils.GetThirdPartyPlatforms()
    if self.platform in platforms:
      return [os.path.join(platforms[self.platform], 'gyp_configuration.gypi')]
    return [os.path.join(self.config_path, self.platform + '.gypi')]

  def GetEnvironmentVariables(self):
    """Returns a dict of environment variables.

    The environment variables are set before calling GYP and can be
    used to manipulate the GYP behavior.

    Returns:
      A dictionary containing environment variables.
    """
    return {}

  def GetVariables(self, config):
    """Returns a dict of GYP variables for the given configuration."""
    _ = config
    return {}

  def GetGeneratorVariables(self, config):
    """Returns a dict of generator variables for the given configuration."""
    _ = config
    return {}


def LoadPlatformConfig(platform):
  """Loads a platform specific configuration.

  The function will use the provided platform name to load
  a python file with a matching name that contains the platform
  specific configuration.

  Args:
    platform: Platform name.

  Returns:
    Instance of a class derived from PlatformConfigBase.
  """
  try:
    logging.debug('Loading platform configuration for "%s".', platform)
    platforms = gyp_utils.GetThirdPartyPlatforms()
    if platform in platforms:
      platform_path = platforms[platform]
      module_path = os.path.join(platform_path, 'gyp_configuration.py')
      platform_module = imp.load_source('platform_module', module_path)
    else:
      module_path = 'config/{}.py'.format(platform)
      platform_module = importlib.import_module('config.{}'.format(platform))
  except ImportError:
    logging.exception('Unable to import "%s".', module_path)
    return None

  if not hasattr(platform_module, 'CreatePlatformConfig'):
    logging.error('"%s" does not contain CreatePlatformConfig.', module_path)
    return None

  return platform_module.CreatePlatformConfig()
