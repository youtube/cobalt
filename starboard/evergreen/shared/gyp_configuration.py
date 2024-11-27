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
"""Starboard Evergreen shared platform configuration."""

import os

from starboard.build import platform_configuration
from starboard.tools.testing import test_filter


class EvergreenConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Evergreen platform configuration."""

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetTestTargets(self):
    tests = super().GetTestTargets()
    tests.extend({
        'cobalt_slot_management_test',
        'crx_file_test',
        'updater_test',
    })
    tests += test_filter.EVERGREEN_TESTS
    return tests
