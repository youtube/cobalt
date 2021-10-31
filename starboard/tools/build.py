#
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

import imp
import importlib
import logging
import os
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import config
from starboard.tools import paths
from starboard.tools import platform
from starboard.tools import download_clang

_STARBOARD_TOOLCHAINS_DIR_KEY = 'STARBOARD_TOOLCHAINS_DIR'
_STARBOARD_TOOLCHAINS_DIR_NAME = 'starboard-toolchains'

# TODO: Rectify consistency of "Build Type" / "Build Config" naming.
_BUILD_CONFIG_KEY = 'BUILD_TYPE'
_BUILD_PLATFORM_KEY = 'BUILD_PLATFORM'
_BUILD_CONFIGURATION_KEY = 'BUILD_CONFIGURATION'


class ClangSpecification(object):
  """A specific version of Clang."""

  def __init__(self, revision, version):
    assert revision
    assert version
    self.revision = revision
    self.version = version


def _CheckConfig(key, raw_value, value):
  if config.IsValid(value):
    return True

  logging.warning("Environment variable '%s' is '%s', which is invalid.", key,
                  raw_value)
  logging.warning('Valid build configurations: %s', config.GetAll())
  return False


def _CheckPlatform(key, raw_value, value):
  if platform.IsValid(value):
    return True

  logging.warning("Environment variable '%s' is '%s', which is invalid.", key,
                  raw_value)
  logging.warning('Valid platforms: %s', platform.GetAll())
  return False


def GetDefaultConfigAndPlatform():
  """Returns a (config_name, platform_name) tuple based on the environment."""
  default_config_name = None
  default_platform_name = None
  if _BUILD_CONFIG_KEY in os.environ:
    raw_config_name = os.environ[_BUILD_CONFIG_KEY]
    config_name = raw_config_name.lower()
    if _CheckConfig(_BUILD_CONFIG_KEY, raw_config_name, config_name):
      default_config_name = config_name

  if _BUILD_PLATFORM_KEY in os.environ:
    raw_platform_name = os.environ[_BUILD_PLATFORM_KEY]
    platform_name = raw_platform_name.lower()
    if _CheckPlatform(_BUILD_PLATFORM_KEY, raw_platform_name, platform_name):
      default_platform_name = platform_name

  if default_config_name and default_platform_name:
    return default_config_name, default_platform_name

  # Only check BUILD_CONFIGURATION if either platform or config is not
  # provided individually, or at least one is invalid.
  if _BUILD_CONFIGURATION_KEY not in os.environ:
    return default_config_name, default_platform_name

  raw_configuration = os.environ[_BUILD_CONFIGURATION_KEY]
  build_configuration = raw_configuration.lower()
  if '_' not in build_configuration:
    logging.warning(
        "Expected a '_' in '%s' and did not find one.  "
        "'%s' must be of the form <platform>_<config>.",
        _BUILD_CONFIGURATION_KEY, _BUILD_CONFIGURATION_KEY)
    return default_config_name, default_platform_name

  platform_name, config_name = build_configuration.split('_', 1)
  if not default_config_name:
    if _CheckConfig(_BUILD_CONFIGURATION_KEY, raw_configuration, config_name):
      default_config_name = config_name

  if not default_platform_name:
    if _CheckPlatform(_BUILD_CONFIGURATION_KEY, raw_configuration,
                      platform_name):
      default_platform_name = platform_name

  return default_config_name, default_platform_name


def GetGyp():
  """Gets the GYP module, loading it, if necessary."""
  if 'gyp' not in sys.modules:
    sys.path.insert(
        0, os.path.join(paths.REPOSITORY_ROOT, 'tools', 'gyp', 'pylib'))
    importlib.import_module('gyp')
  return sys.modules['gyp']


def GypDebugOptions():
  """Returns all valid GYP debug options."""
  debug_modes = []
  gyp = GetGyp()
  for name in dir(gyp):
    if name.startswith('DEBUG_'):
      debug_modes.append(getattr(gyp, name))
  return debug_modes


def GetToolchainsDir():
  """Gets the directory that contains all downloaded toolchains."""
  home_dir = os.environ.get('HOME')
  toolchains_dir = os.path.realpath(
      os.getenv(_STARBOARD_TOOLCHAINS_DIR_KEY,
                os.path.join(home_dir, _STARBOARD_TOOLCHAINS_DIR_NAME)))

  if not os.path.isdir(toolchains_dir):
    # Ensure the toolchains directory exists.
    os.mkdir(toolchains_dir)

  return toolchains_dir


def _GetClangBasePath(clang_spec):
  return os.path.join(GetToolchainsDir(),
                      'x86_64-linux-gnu-clang-chromium-' + clang_spec.revision)


def _GetClangBinPath(clang_spec):
  return os.path.join(_GetClangBasePath(clang_spec), 'bin')


def EnsureClangAvailable(clang_spec):
  """Ensure the expected version of Clang is available."""

  # Run the Clang update tool to ensure correct version of Clang,
  # then check that Clang is in the path.
  base_dir = _GetClangBasePath(clang_spec)
  download_clang.UpdateClang(target_dir=base_dir, revision=clang_spec.revision)

  # update.sh downloads Clang to this path.
  clang_bin = os.path.join(_GetClangBinPath(clang_spec), 'clang')

  if not os.path.exists(clang_bin):
    raise RuntimeError('Clang not found.')

  return _GetClangBasePath(clang_spec)


def GetHostCompilerEnvironment(clang_spec, build_accelerator):
  """Return the host compiler toolchain environment."""
  toolchain_dir = EnsureClangAvailable(clang_spec)
  toolchain_bin_dir = os.path.join(toolchain_dir, 'bin')

  cc_clang = os.path.join(toolchain_bin_dir, 'clang')
  cxx_clang = os.path.join(toolchain_bin_dir, 'clang++')
  host_clang_environment = {
      'CC_host': build_accelerator + ' ' + cc_clang,
      'CXX_host': build_accelerator + ' ' + cxx_clang,
      'LD_host': cxx_clang,
      'ARFLAGS_host': 'rcs',
      'ARTHINFLAGS_host': 'rcsT',
  }
  return host_clang_environment


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
      platform_path = os.path.join(paths.REPOSITORY_ROOT,
                                   platform.Get(platform_name).path)
      module_path = os.path.join(platform_path, 'gyp_configuration.py')
      if not _ModuleLoaded('platform_module', module_path):
        platform_module = imp.load_source('platform_module', module_path)
      else:
        platform_module = sys.modules['platform_module']
    else:
      module_path = os.path.join('config', '%s.py' % platform_name)
      platform_module = importlib.import_module('config.%s' % platform_name)
  except (ImportError, IOError):
    logging.exception('Unable to import "%s".', module_path)
    return None

  if not hasattr(platform_module, 'CreatePlatformConfig'):
    logging.error('"%s" does not contain CreatePlatformConfig.', module_path)
    return None

  try:
    platform_configuration = platform_module.CreatePlatformConfig()
    platform_configuration.SetDirectory(platform_path)
    return platform_configuration
  except RuntimeError:
    logging.exception('Exception in CreatePlatformConfig.')
    return None


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
