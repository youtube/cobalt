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
"""Starboard Android shared platform configuration for gyp_cobalt."""

from __future__ import print_function

import importlib.util
import os

from starboard.build.platform_configuration import PlatformConfiguration


class AndroidConfiguration(PlatformConfiguration):
  """Starboard Android platform configuration."""

  def __init__(self, platform):
    super().__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'launcher.py'))
    spec = importlib.util.spec_from_file_location('launcher', module_path)
    launcher_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(launcher_module)
    return launcher_module
