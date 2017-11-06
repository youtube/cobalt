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
"""Classes related to building and installing platform-specific packages."""

import abc
import importlib
import logging
import os

import _env  # pylint: disable=unused-import
import starboard
from starboard.tools import platform


def _ImportModule(path, root_module, module_name=None):
  """Load a platform specific python module using importlib.

  Load the python module named |module_name| relative to |root_module|.
  Args:
    path: Path to the platform
    root_module: An already-loaded module
    module_name: Name of a python module to load. If None, load the platform
        directory as a python module.
  Returns:
    A module loaded with importlib.import_module
  Throws:
    ImportError if the module fails to be loaded.
  """
  # From the relative path to the |root_module|'s directory, construct a full
  # python package name and attempt to load it.
  relative_path = os.path.relpath(path, os.path.dirname(root_module.__file__))
  full_path = os.path.join(root_module.__name__, relative_path)

  # This normpath may collapse out the root module. This is generally OK as long
  # as we stay within the workspace, as _env will add the workspace root to the
  # system import path.
  components = os.path.normpath(full_path).split(os.sep)
  if module_name:
    components.append(module_name)
  full_package_name = '.'.join(components)
  return importlib.import_module(full_package_name)


def _GetPackageClass(platform_info):
  """Loads the package class associated with the given platform.PlatformInfo."""
  try:
    module = _ImportModule(platform_info.path, starboard)
  except ImportError as e:
    logging.debug('Failed to import module for platform %s with error: %s',
                  platform_info.name, e)
    return None

  if not hasattr(module, 'Package'):
    return None

  return module.Package


def _GetPlatformInfosDict():
  """Find Starboard ports that support building packages.

  Returns:
    A dict of [platform_name, Class] where Class inherits from PackageBase
  """
  packager_modules = {}
  for platform_name in platform.GetAll():
    platform_info = platform.Get(platform_name)
    # From the relative path to the starboard directory, construct a full
    # python package name and attempt to load it.
    package_class = _GetPackageClass(platform_info)
    if not package_class:
      continue
    # Populate a mapping from platform name to the module containing the
    # Package class.
    try:
      for supported_name in package_class.SupportedPlatforms():
        if supported_name in packager_modules:
          logging.warning('Packager for %s is defined in multiple modules.',
                          supported_name)
        else:
          packager_modules[supported_name] = platform_info
    except Exception as e:  # pylint: disable=broad-except
      # Catch all exceptions to avoid an error in one platform's Packager
      # halting the script for other platforms' packagers.
      logging.warning('Exception iterating supported platform for platform '
                      '%s: %s.', platform_info.name, e)

  return packager_modules


class PackageBase(object):
  """Represents a build package that exists on the local filesystem."""
  __metaclass__ = abc.ABCMeta

  def __init__(self, source_dir, output_dir):
    """Initialize common paths for building Packages.

    Args:
      source_dir: The directory containing the application to be packaged.
      output_dir: The directory into which the package should be placed.
    """
    self.source_dir = source_dir
    self.output_dir = output_dir

  @abc.abstractmethod
  def Install(self, targets=None):
    """Install the package to the specified list of targets.

    Args:
      targets: A list of targets to install the package to, or None on platforms
          that support installing to a default target.

    This method can be overridden to implement platform-specific steps to
    install the package for that platform.
    """
    del targets

  @classmethod
  def AddArguments(cls, arg_parser):
    """Add platform-specific command-line arguments to the ArgumentParser.

    Platforms that require additional arguments to configure building a package
    can override this method and add them to |arg_parser|.
    Args:
      arg_parser: An ArgumentParser object.
    """
    del cls, arg_parser

  @classmethod
  def ExtractArguments(cls, options):
    """Extract arguments from an ArgumentParser's namespace object.

    Platforms that add additional arguments can override this method to extract
    the options and add them to a dict that will be passed to the Package
    constructor.
    Args:
      options: A namespace object returned from ArgumentParser.parse_args
    Returns:
      A dict of kwargs to be passed to the Package constructor.
    """
    del cls, options
    return {}


class Packager(object):
  """Top level class for building a package."""

  def __init__(self):
    self.platform_infos = _GetPlatformInfosDict()

  def SupportedPlatforms(self):
    """Get a list of platforms for which a package can be built."""
    return self.platform_infos.keys()

  def GetPlatformInfo(self, platform_name):
    return self.platform_infos.get(platform_name, None)

  def GetApplicationPackageInfo(self, platform_name, applciation_name):
    """Get application-specific packaging information."""
    platform_info = self.GetPlatformInfo(platform_name)
    try:
      return _ImportModule(platform_info.path, starboard,
                           '%s.package' % applciation_name)
    except ImportError as e:
      # No package parameters specified for this platform.
      logging.debug('Failed to import cobalt.package: %s', e)
    return None

  def BuildPackage(self, platform_name, source_dir, output_dir, **kwargs):
    """Build a package for the specified platform.

    Args:
      platform_name: The platform for which a package should be built.
      source_dir: The directory containing the application to be packaged.
      output_dir: The directory into which the package files should be placed.
      **kwargs: Platform-specific arguments.
    Returns:
      A PackageBase instance.
    """
    package_class = _GetPackageClass(self.platform_infos[platform_name])
    return package_class(source_dir=source_dir, output_dir=output_dir, **kwargs)

  def AddPlatformArguments(self, platform_name, argparser):
    package_class = _GetPackageClass(self.platform_infos[platform_name])
    package_class.AddArguments(argparser)

  def ExtractPlatformArguments(self, platform_name, options):
    package_class = _GetPackageClass(self.platform_infos[platform_name])
    return package_class.ExtractArguments(options)
