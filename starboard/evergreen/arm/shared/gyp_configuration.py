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
"""Starboard Evergreen ARM shared platform configuration for gyp_cobalt."""

import os.path

from starboard.evergreen.shared import gyp_configuration as shared_configuration


class EvergreenArmConfiguration(shared_configuration.EvergreenConfiguration):
  """Starboard Evergreen ARM platform configuration."""

  def __init__(self, platform):
    super().__init__(platform)

    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))
