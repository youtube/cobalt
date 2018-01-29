# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Base application-platform build configuration.

If applications wish to customize or extend build configuration information by
platform, they should add an <application-name>/configuration.py file to the
port directory of the platform they wish to customize
(e.g. linux/x64x11/cobalt/configuration.py for the `cobalt` application and the
`linux-x64x11` platform). This module should contain an class that extends the
class defined here.
"""

import os


class ApplicationConfiguration(object):
  """Base build configuration class for all Starboard applications.

  Should be derived by application configurations.
  """

  def __init__(self, platform_configuration, application_name,
               application_directory):
    """Initialize ApplicationConfiguration.

    Args:
      platform_configuration: An instance of StarboardBuildConfiguration for the
                              platform being built.
      application_name: The name of the application that is loading this
                        configuration.
      application_directory: The absolute path of the directory containing the
                             application configuration being loaded.
    """
    self._platform_configuration = platform_configuration
    self._application_name = application_name
    self._application_directory = application_directory

  def GetName(self):
    """Gets the application name."""
    return self._application_name

  def GetDirectory(self):
    """Gets the directory of the application configuration."""
    return self._application_directory

  def GetPreIncludes(self):
    """Get a list of absolute paths to gypi files to include in order.

    These files will be included by GYP before any processed .gyp file. The
    files returned by this function will be included by GYP before any other
    gypi files.

    Returns:
      An ordered list containing absolute paths to .gypi files.
    """
    return []

  def GetPostIncludes(self):
    """Get a list of absolute paths to gypi files to include in order.

    These files will be included by GYP before any processed .gyp file. The
    files return by this function will be included by GYP after any other gypi
    files.

    Returns:
      An ordered list containing absolute paths to .gypi files.
    """
    standard_gypi_path = os.path.join(self.GetDirectory(), 'configuration.gypi')
    if os.path.isfile(standard_gypi_path):
      return [standard_gypi_path]
    return []

  def GetEnvironmentVariables(self):
    """Get a Mapping of environment variable overrides.

    The environment variables returned by this function are set before calling
    GYP or GN and can be used to manipulate their behavior. They will override
    any environment variables of the same name in the calling environment, or
    any that are set by default or by the platform.

    Returns:
      A dictionary containing environment variables.
    """
    return {}

  def GetVariables(self, config_name):
    """Get a Mapping of GYP/GN variable overrides.

    Get a Mapping of GYP/GN variable names to values. Any defined values will
    override any values defined by default or by the platform. GYP or GN files
    can then have conditionals that check the values of these variables.

    Args:
      config_name: The name of the starboard.tools.config build type.

    Returns:
      A Mapping of GYP/GN variables to be merged into the global Mapping
      provided to GYP/GN.
    """
    del config_name
    return {}

  def GetGeneratorVariables(self, config_name):
    """Get a Mapping of generator variable overrides.

    Args:
      config_name: The name of the starboard.tools.config build type.

    Returns:
      A Mapping of generator variable names and values.
    """
    del config_name
    return {}

  def GetTestEnvVariables(self):
    """Gets a dict of environment variables needed by unit test binaries."""
    return {}

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized starboard.tools.testing.TestFilter objects.
    """
    return []

  def GetTestTargets(self):
    """Gets all tests to be run in a unit test run.

    Returns:
      A list of strings of test target names.
    """
    return []

  def GetDefaultTargetBuildFile(self):
    """Gets the build file to build by default."""
    return None
