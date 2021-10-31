#!/usr/bin/python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Classes that package common and related command-line arguments for Starboard tooling.

These classes provide the opportunity for cleaner, more understandable, and more
maintainable code. Each class stores packages of related parameters, most often
parsed from the command-line, that are frequently used by Starboard tooling.
"""


class PlatformConfigParams(object):
  """A class to store platform configuration information.

  This class provides the base information needed for building.
  """

  def __init__(self):
    """PlatformConfigParams constructor."""
    self.platform = None
    self.config = None
    self.loader_platform = None
    self.loader_config = None

  def IsEvergreen(self):
    """Returns whether PlatformConfigParams is for Evergreen."""
    return self.loader_platform and self.loader_config


class LauncherParams(PlatformConfigParams):
  """A class that extends PlatformConfigParams with device info for launchers.

  This class provides all of the base information needed by Starboard tooling to
  do things like run launchers.
  """

  def __init__(self):
    """LauncherParams constructor."""
    super(LauncherParams, self).__init__()
    self.device_id = None
    self.out_directory = None
    self.loader_out_directory = None
    self.target_params = None
