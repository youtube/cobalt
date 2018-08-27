# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Creator Ci20 X11 mozjs platform configuration."""

from starboard.contrib.creator.shared import gyp_configuration as shared_configuration
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch


class CreatorCI20X11MozjsConfiguration(shared_configuration.CreatorConfiguration):
  """Starboard Creator X11 mozjs platform configuration."""

  def __init__(self, platform_name='creator-ci20x11-mozjs'):
    super(CreatorCI20X11MozjsConfiguration, self).__init__(platform_name)

  def GetVariables(self, config_name):
    variables = super(CreatorCI20X11MozjsConfiguration, self).GetVariables(
        config_name)
    variables.update({
        'javascript_engine': 'mozjs-45',
        'cobalt_enable_jit': 0,
    })

    return variables

def CreatePlatformConfig():
  return CreatorCI20X11MozjsConfiguration()
