# Copyright 2018 Google Inc. All Rights Reserved.
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
"""Starboard Raspberry Pi 2 platform configuration for gyp_cobalt."""

import importlib

# pylint: disable=invalid-name
Raspi2PlatformConfig = importlib.import_module(
    'starboard.raspi.2.gyp_configuration').Raspi2PlatformConfig


class Raspi2MozjsPlatformConfig(Raspi2PlatformConfig):

  def __init__(self, platform):
    super(Raspi2MozjsPlatformConfig, self).__init__(platform)

  def GetVariables(self, config_name):
    variables = super(Raspi2MozjsPlatformConfig, self).GetVariables(config_name)
    variables.update({
        'javascript_engine': 'mozjs-45',
        'cobalt_enable_jit': 0,
    })
    return variables


def CreatePlatformConfig():
  return Raspi2MozjsPlatformConfig('raspi-2-mozjs')
