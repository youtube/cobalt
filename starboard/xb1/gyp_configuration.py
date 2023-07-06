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
"""Starboard Xbox One platform configuration for gyp_cobalt."""
import imp  # pylint: disable=deprecated-module
import logging
import os
from starboard.shared.win32 import gyp_configuration as gyp_configuration_win32


def CreatePlatformConfig():
  try:
    xb1_config = Xb1Configuration('xb1')
    return xb1_config
  except RuntimeError as e:
    logging.critical(e)
    return None


class Xb1Configuration(gyp_configuration_win32.Win32SharedConfiguration):
  """Starboard XB1 platform configuration."""

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'launcher.py'))
    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def GetDeployPathPatterns(self):
    """example src/out/xb1_devel/appx"""
    return ['appx/*']  # Overinclude
