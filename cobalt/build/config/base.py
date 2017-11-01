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
import sys

import _env  # pylint: disable=unused-import
import cobalt.tools.webdriver_benchmark_config as wb_config
from starboard.tools import platform
from starboard.tools.config import Config


class PlatformConfigBase(object):
  """Base platform configuration class.

  Should be derived by platform specific configurations.
  """

  def __init__(self, platform_name, asan_enabled_by_default=False):
    self.platform = platform_name
    self.config_path = os.path.abspath(os.path.dirname(__file__))
    self.asan_default = 1 if asan_enabled_by_default else 0

  def GetBuildFormat(self):
    """Returns the desired build format."""
    return 'ninja'

  def GetBuildFlavor(self):
    """Returns the gyp build "flavor"."""
    return self.platform

  def GetIncludes(self):
    """Returns a list of gypi files.

    These files will be included by GYP before any processed .gyp file.

    Returns:
        A list containing paths to .gypi files.
    """
    platform_info = platform.Get(self.platform)
    if not platform_info:
      return []

    return [os.path.join(platform_info.path, 'gyp_configuration.gypi')]

  def GetEnvironmentVariables(self):
    """Returns a dict of environment variables.

    The environment variables are set before calling GYP and can be
    used to manipulate the GYP behavior.

    Returns:
      A dictionary containing environment variables.
    """
    return {}

  def GetVariables(self, config_name, use_clang=0):
    """Returns a dict of GYP variables for the given configuration."""
    use_asan = 0
    use_tsan = 0
    vr_enabled = 0
    if use_clang:
      use_tsan = int(os.environ.get('USE_TSAN', 0))
      # Enable ASAN by default for debug and devel builds only if USE_TSAN was
      # not set to 1 in the environment.
      use_asan_default = self.asan_default if not use_tsan and config_name in (
          Config.DEBUG, Config.DEVEL) else 0
      use_asan = int(os.environ.get('USE_ASAN', use_asan_default))

      # Set environmental variable to enable_vr: 'USE_VR'
      # Terminal: `export {varname}={value}`
      # Note: must also edit gyp_configuration.gypi per internal instructions.
      vr_on_by_default = 0
      vr_enabled = int(os.environ.get('USE_VR', vr_on_by_default))

    if use_asan == 1 and use_tsan == 1:
      raise RuntimeError('ASAN and TSAN are mutually exclusive')

    if use_asan:
      logging.info('Using ASan Address Sanitizer')
    if use_tsan:
      logging.info('Using TSan Thread Sanitizer')

    variables = {
        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,
        'clang': use_clang,
        # Whether to build with clang's Address Sanitizer instrumentation.
        'use_asan': use_asan,
        # Whether to build with clang's Thread Sanitizer instrumentation.
        'use_tsan': use_tsan,
        # Whether to enable VR.
        'enable_vr': vr_enabled,
    }
    return variables

  def GetGeneratorVariables(self, config_name):
    """Returns a dict of generator variables for the given configuration."""
    del config_name
    return {}

  def GetToolchain(self):
    """Returns the instance of the toolchain implementation class."""
    return None

  def GetTargetToolchain(self):
    """Returns a list of target tools."""
    # TODO: If this method throws |NotImplementedError|, GYP will fall back to
    #       the legacy toolchain. Once all platforms are migrated to the
    #       abstract toolchain, this method should be made |@abstractmethod|.
    raise NotImplementedError()

  def GetHostToolchain(self):
    """Returns a list of host tools."""
    # TODO: If this method throws |NotImplementedError|, GYP will fall back to
    #       the legacy toolchain. Once all platforms are migrated to the
    #       abstract toolchain, this method should be made |@abstractmethod|.
    raise NotImplementedError()

  def GetTestEnvVariables(self):
    """Gets a dict of environment variables needed by unit test binaries."""
    return {}

  def WebdriverBenchmarksEnabled(self):
    """Determines if webdriver benchmarks are enabled or not.

    Returns:
      True if webdriver benchmarks can run on this platform, False if not.
    """
    return False

  def GetDefaultSampleSize(self):
    return wb_config.STANDARD_SIZE

  def GetWebdriverBenchmarksTargetParams(self):
    """Gets command line params to pass to the Cobalt executable."""
    return []

  def GetWebdriverBenchmarksParams(self):
    """Gets command line params to pass to the webdriver benchmark script."""
    return []


def _ModuleLoaded(module_name, module_path):
  if module_name not in sys.modules:
    return False
  # Sometimes one of these has .pyc and the other has .py, but we don't care.
  extensionless_loaded_path = os.path.splitext(
      os.path.abspath(sys.modules['platform_module'].__file__))[0]
  extensionless_module_path = os.path.splitext(os.path.abspath(module_path))[0]
  return extensionless_loaded_path == extensionless_module_path


def _LoadPlatformConfig(platform_name):
  """Loads a platform specific configuration.

  The function will use the provided platform name to load
  a python file with a matching name that contains the platform
  specific configuration.

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from PlatformConfigBase.
  """
  try:
    logging.debug('Loading platform configuration for "%s".', platform_name)
    if platform.IsValid(platform_name):
      platform_path = platform.Get(platform_name).path
      module_path = os.path.join(platform_path, 'gyp_configuration.py')
      if not _ModuleLoaded('platform_module', module_path):
        platform_module = imp.load_source('platform_module', module_path)
      else:
        platform_module = sys.modules['platform_module']
    else:
      module_path = os.path.join('config', '%s.py' % platform_name)
      platform_module = importlib.import_module('config.%s' % platform_name)
  except ImportError:
    logging.exception('Unable to import "%s".', module_path)
    return None

  if not hasattr(platform_module, 'CreatePlatformConfig'):
    logging.error('"%s" does not contain CreatePlatformConfig.', module_path)
    return None

  return platform_module.CreatePlatformConfig()


# Global cache of the platform configurations, so that platform config objects
# are only created once.
_PLATFORM_CONFIG_DICT = {}


def GetPlatformConfig(platform_name):
  """Returns a platform specific configuration.

  This function will return a cached platform configuration object, loading it
  if it doesn't exist via a call to _LoadPlatformConfig().

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from PlatformConfigBase.
  """

  if platform_name not in _PLATFORM_CONFIG_DICT:
    _PLATFORM_CONFIG_DICT[platform_name] = _LoadPlatformConfig(platform_name)

  return _PLATFORM_CONFIG_DICT[platform_name]
