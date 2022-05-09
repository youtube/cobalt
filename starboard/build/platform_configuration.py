# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Base platform build configuration."""

import imp  # pylint: disable=deprecated-module
import inspect
import logging
import os

from starboard.build.application_configuration import ApplicationConfiguration
import starboard_configuration  # Should be at root of source tree


def _GetApplicationConfigurationClass(application_path):
  """Gets the ApplicationConfiguration class from the given path."""
  if not os.path.isdir(application_path):
    return None

  application_configuration_path = os.path.join(application_path,
                                                'configuration.py')
  if not os.path.isfile(application_configuration_path):
    return None

  application_configuration = imp.load_source('application_configuration',
                                              application_configuration_path)
  for name, cls in inspect.getmembers(application_configuration):
    if not inspect.isclass(cls):
      continue
    if issubclass(cls, ApplicationConfiguration):
      logging.debug('Found ApplicationConfiguration: %s in %s', name,
                    application_configuration_path)
      return cls

  return None


class PlatformConfiguration(object):
  """Base platform build configuration class for all Starboard platforms.

  Should be derived by platform specific configurations.
  """

  def __init__(self, platform_name):
    self._platform_name = platform_name
    self._directory = os.path.realpath(os.path.dirname(__file__))
    self._application_configuration = None
    self._application_configuration_search_path = [self._directory]

  def GetName(self):
    """Returns the platform name."""
    return self._platform_name

  def GetDirectory(self):
    """Returns the directory of the platform configuration."""
    return self._directory

  # This happens post-construction to maintain compatibility with legacy
  # configurations.
  # TODO: Find some other way to do this.
  def SetDirectory(self, directory):
    """Sets the directory of the platform configuration."""
    try:
      i = self._application_configuration_search_path.index(self._directory)
      self._application_configuration_search_path.pop(i)
    except ValueError:
      i = 0
    self._directory = directory
    self._application_configuration_search_path.insert(i, self._directory)

  def AppendApplicationConfigurationPath(self, path):
    """Appends a path to search for ApplicationConfiguration modules."""
    self._application_configuration_search_path.append(path)

  def GetApplicationConfiguration(self, application_name):
    """Get the instance of ApplicationConfiguration for this app, if any.

    Looks for a class deriving from ApplicationConfiguration defined for
    this platform. If it finds one, this function will load, instantiate, and
    return the configuration. Otherwise, it will return None.

    Args:
      application_name: The name of the application to load, in a canonical
        filesystem-friendly form.

    Returns:
      An instance of ApplicationConfiguration defined for the application being
      loaded.
    """
    if not self._application_configuration:
      application_path = os.path.join(self.GetDirectory(), application_name)
      for directory in self._application_configuration_search_path:
        configuration_path = os.path.join(directory, application_name)
        logging.debug('Searching for ApplicationConfiguration in %s',
                      configuration_path)
        configuration_class = _GetApplicationConfigurationClass(
            configuration_path)
        if configuration_class:
          logging.info(
              'Using platform-specific ApplicationConfiguration for '
              '%s.', application_name)
          break

      if not configuration_class:
        configuration_class = starboard_configuration.APPLICATIONS.get(
            application_name)
        if configuration_class:
          logging.info('Using default ApplicationConfiguration for %s.',
                       application_name)

      if not configuration_class:
        logging.info('Using base ApplicationConfiguration.')
        configuration_class = ApplicationConfiguration

      self._application_configuration = configuration_class(
          self, application_name, application_path)

    assert self._application_configuration
    return self._application_configuration

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return self.GetDirectory()

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(self.GetLauncherPath(), 'launcher.py'))
    try:
      return imp.load_source('launcher', module_path)
    except (IOError, ImportError, RuntimeError) as error:
      logging.error('Unable to load launcher module from %s.', module_path)
      logging.error(error)
      return None

  def GetTestEnvVariables(self):
    """Gets a dict of environment variables needed by unit test binaries."""
    return {}

  def GetDeployPathPatterns(self):
    """Gets deployment paths patterns for files to be included for deployment.

    Example: ['deploy/*.exe', 'content/*']

    Returns:
      A list of path wildcard patterns within the PRODUCT_DIR
      (src/out/<PLATFORM>_<CONFIG>) that need to be deployed in order for the
      platform launcher to run target executable(s).
    """
    raise NotImplementedError()

  def GetTestTargets(self):
    """Gets all tests to be run in a unit test run.

    Returns:
      A list of strings of test target names.
    """
    return [
        'app_key_files_test',
        'app_key_test',
        'drain_file_test',
        'elf_loader_test',
        'installation_manager_test',
        'nplb',
        'player_filter_tests',
        'slot_management_test',
        'starboard_platform_tests',
    ]
