#
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
#
"""Build related constants and helper functions."""

import logging
import os

import _env  # pylint: disable=unused-import
import starboard.tools.config
import starboard.tools.platform


# TODO: Rectify consistency of "Build Type" / "Build Config" naming.
_BUILD_CONFIG_KEY = 'BUILD_TYPE'
_BUILD_PLATFORM_KEY = 'BUILD_PLATFORM'
_BUILD_CONFIGURATION_KEY = 'BUILD_CONFIGURATION'


def _CheckConfig(key, raw_value, value):
  if starboard.tools.config.IsValid(value):
    return True

  logging.warning("Environment variable '%s' is '%s', which is invalid.",
                  key, raw_value)
  logging.warning('Valid build configurations: %s',
                  starboard.tools.config.GetAll())
  return False


def _CheckPlatform(key, raw_value, value):
  if starboard.tools.platform.IsValid(value):
    return True

  logging.warning("Environment variable '%s' is '%s', which is invalid.",
                  key, raw_value)
  logging.warning('Valid platforms: %s', starboard.tools.platform.GetAll())
  return False


class Config(starboard.tools.config.Config):
  pass


def GetDefaultConfigAndPlatform():
  """Returns a (config, platform) tuple based on the environment."""
  default_config = None
  default_platform = None
  if _BUILD_CONFIG_KEY in os.environ:
    raw_config = os.environ[_BUILD_CONFIG_KEY]
    config = raw_config.lower()
    if _CheckConfig(_BUILD_CONFIG_KEY, raw_config, config):
      default_config = config

  if _BUILD_PLATFORM_KEY in os.environ:
    raw_platform = os.environ[_BUILD_PLATFORM_KEY]
    platform = raw_platform.lower()
    if _CheckPlatform(_BUILD_PLATFORM_KEY, raw_platform, platform):
      default_platform = platform

  if default_config and default_platform:
    return default_config, default_platform

  # Only check BUILD_CONFIGURATION if either platform or config is not
  # provided individually, or at least one is invalid.
  if _BUILD_CONFIGURATION_KEY not in os.environ:
    return default_config, default_platform

  raw_configuration = os.environ[_BUILD_CONFIGURATION_KEY]
  build_configuration = raw_configuration.lower()
  if '_' not in build_configuration:
    logging.warning("Expected a '_' in '%s' and did not find one.  "
                    "'%s' must be of the form <platform>_<config>.",
                    _BUILD_CONFIGURATION_KEY, _BUILD_CONFIGURATION_KEY)
    return default_config, default_platform

  platform, config = build_configuration.split('_', 1)
  if not default_config:
    if _CheckConfig(_BUILD_CONFIGURATION_KEY, raw_configuration, config):
      default_config = config

  if not default_platform:
    if _CheckPlatform(_BUILD_CONFIGURATION_KEY, raw_configuration,
                      platform):
      default_platform = platform

  return default_config, default_platform
