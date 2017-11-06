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
"""Starboard Raspberry Pi 0 Cobalt configuration."""

import cobalt.tools.webdriver_benchmark_config as wb_config
from starboard.raspi.shared.cobalt import configuration as shared_configuration


class CobaltRaspiZeroConfiguration(
    shared_configuration.CobaltRaspiConfiguration):
  """Starboard Raspberry Pi 0 Cobalt configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltRaspiZeroConfiguration, self).__init__(
        platform_configuration, application_name, application_directory)

  def GetDefaultSampleSize(self):
    return wb_config.REDUCED_SIZE
