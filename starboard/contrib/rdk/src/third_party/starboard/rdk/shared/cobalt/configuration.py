# Copyright 2020 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2017-2020 The Cobalt Authors. All Rights Reserved.
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
"""Starboard RDK Cobalt configuration."""

import os

from cobalt.build import cobalt_configuration

class CobaltRDKConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard RDK Cobalt configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltRDKConfiguration,
          self).__init__(platform_configuration, application_name,
                         application_directory)
    self.enable_debugger = os.environ.get('COBALT_ENABLE_DEBUGGER', '0')
    self.enable_webdriver = os.environ.get('COBALT_ENABLE_WEBDRIVER', '0')

  def GetVariables(self, config_name):
    variables = super(CobaltRDKConfiguration, self).GetVariables(config_name)

    variables.update({
        'enable_debugger': self.enable_debugger,
        'enable_webdriver': self.enable_webdriver,
    })
    return variables
