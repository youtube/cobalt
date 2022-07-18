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


class EvergreenConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Evergreen platform configuration."""

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetTestTargets(self):
    tests = super(EvergreenConfiguration, self).GetTestTargets()
    tests.extend({'cobalt_slot_management_test', 'updater_test'})
    return [test for test in tests if test not in self.__FORBIDDEN_TESTS]

  __FORBIDDEN_TESTS = [  # pylint: disable=invalid-name
      # elf_loader_test and installation_manager_test are explicitly tests that
      # validate the correctness of the underlying platform. We should not be
      # running these tests in Evergreen mode, and instead will rely on the
      # platform to test this directly instead.
      'elf_loader_test',
      'installation_manager_test',
  ]
