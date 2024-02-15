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
"""Base application-platform build configuration.

If applications wish to customize or extend build configuration information by
platform, they should add an <application-name>/configuration.py file to the
port directory of the platform they wish to customize
(e.g. linux/x64x11/cobalt/configuration.py for the `cobalt` application and the
`linux-x64x11` platform). This module should contain an class that extends the
class defined here.
"""


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

  def GetTestBlackBoxTargets(self):
    """Gets all tests to be run in black box test run.

    Returns:
      A list of strings of black box test target names.
    """
    return []
