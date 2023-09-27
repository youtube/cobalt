# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Starboard RDK platform configuration."""

import os

from starboard.build import platform_configuration


def CreatePlatformConfig():
  return RdkPlatformConfig('rdk')


class RdkPlatformConfig(platform_configuration.PlatformConfiguration):
  """Starboard RDK platform configuration."""

  def __init__(self, platform):
    super().__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)
