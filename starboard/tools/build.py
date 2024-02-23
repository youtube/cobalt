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

import importlib
import importlib.machinery
import importlib.util
import logging
import os
import sys

from starboard.build.platforms import PLATFORMS
from starboard.tools import paths

_STARBOARD_TOOLCHAINS_DIR_KEY = 'STARBOARD_TOOLCHAINS_DIR'
_STARBOARD_TOOLCHAINS_DIR_NAME = 'starboard-toolchains'


class ClangSpecification(object):
  """A specific version of Clang."""

  def __init__(self, revision, version):
    assert revision
    assert version
    self.revision = revision
    self.version = version


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


def GetClangBinPath(clang_spec):
  return os.path.join(_GetClangBasePath(clang_spec), 'bin')


def _ModuleLoaded(module_name, module_path):
  if module_name not in sys.modules:
    return False
  # Sometimes one of these has .pyc and the other has .py, but we don't care.
  extensionless_loaded_path = os.path.splitext(
      os.path.abspath(sys.modules['platform_module'].__file__))[0]
  extensionless_module_path = os.path.splitext(os.path.abspath(module_path))[0]
  return extensionless_loaded_path == extensionless_module_path


def _LoadPlatformModule(platform_name, file_name, function_name):
  """Loads a platform module.

  The function will use the provided platform name to load
  a python file with a matching name that contains the module.

  Args:
    platform_name: Platform name.
    file_name: The file that contains the module. It can be
               gyp_configuration.py or test_filters.py
    function_name: The function name of the module.
                   For gyp_configuration.py, it is CreatePlatformConfig.
                   For test_filters.py, it is GetTestFilters

  Returns: Instance of a class derived from module path.
  """
  try:
    logging.debug('Loading platform %s for "%s".', file_name, platform_name)
    if platform_name in PLATFORMS:
      platform_path = os.path.join(paths.REPOSITORY_ROOT,
                                   PLATFORMS[platform_name])
      module_path = os.path.join(platform_path, file_name)
      if not _ModuleLoaded('platform_module', module_path):
        loader = importlib.machinery.SourceFileLoader('platform_module',
                                                      module_path)
        spec = importlib.util.spec_from_file_location(
            'platform_module', module_path, loader=loader)
        platform_module = importlib.util.module_from_spec(spec)
        loader.exec_module(platform_module)
      else:
        platform_module = sys.modules['platform_module']
    else:
      module_path = os.path.join('config', f'{platform_name}.py')
      platform_module = importlib.import_module(f'config.{platform_name}')
  except (ImportError, IOError):
    logging.exception('Unable to import "%s".', module_path)
    return None

  if not hasattr(platform_module, function_name):
    logging.error('"%s" does not contain %s.', module_path, function_name)
    return None

  try:
    platform_attr = getattr(platform_module, function_name)()
    return platform_attr, platform_path
  except RuntimeError:
    logging.exception('Exception in %s.', function_name)
    return None


def _LoadPlatformConfig(platform_name):
  """Loads a platform specific configuration.

  The function will use the provided platform name to load
  a python file with a matching name that contains the platform
  specific configuration.

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from PlatformConfiguration.
  """
  platform_configuration, platform_path = _LoadPlatformModule(
      platform_name, 'gyp_configuration.py', 'CreatePlatformConfig')
  platform_configuration.SetDirectory(platform_path)
  return platform_configuration


def _LoadPlatformTestFilters(platform_name):
  """Loads the platform specific test filters.

  The function will use the provided platform name to load
  a python file with a matching name that contains the platform
  specific test filters.

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from TestFilters.
  """
  platform_test_filters, _ = _LoadPlatformModule(platform_name,
                                                 'test_filters.py',
                                                 'CreateTestFilters')
  return platform_test_filters


# Global cache of the platform configurations, so that platform config objects
# are only created once.
_PLATFORM_CONFIG_DICT = {}
_PLATFORM_TEST_FILTERS_DICT = {}


def GetPlatformConfig(platform_name):
  """Returns a platform specific configuration.

  This function will return a cached platform configuration object, loading it
  if it doesn't exist via a call to _LoadPlatformConfig().

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from PlatformConfiguration.
  """

  if platform_name not in _PLATFORM_CONFIG_DICT:
    _PLATFORM_CONFIG_DICT[platform_name] = _LoadPlatformConfig(platform_name)

  return _PLATFORM_CONFIG_DICT[platform_name]


def GetPlatformTestFilters(platform_name):
  """Returns a platform specific test filters.

  Args:
    platform_name: Platform name.

  Returns:
    Instance of a class derived from TestFilters.
  """

  if platform_name not in _PLATFORM_TEST_FILTERS_DICT:
    _PLATFORM_TEST_FILTERS_DICT[platform_name] = _LoadPlatformTestFilters(
        platform_name)

  return _PLATFORM_TEST_FILTERS_DICT[platform_name]
