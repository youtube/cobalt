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
"""Starboard win-win32 platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp
import logging
import os
import sys

# Import the shared win platform configuration.
sys.path.append(
    os.path.realpath(
        os.path.join(
            os.path.dirname(__file__), os.pardir,
            os.pardir, 'shared', 'win32')))
import gyp_configuration

def CreatePlatformConfig():
  try:
    win_lib_config = WinWin32PlatformConfig('win-win32')
    return win_lib_config
  except RuntimeError as e:
    logging.critical(e)
    return None

class WinWin32PlatformConfig(gyp_configuration.PlatformConfig):
  """Starboard win-32 platform configuration."""

  def __init__(self, platform):
    super(WinWin32PlatformConfig, self).__init__(platform)

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 'launcher.py'))

    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module
