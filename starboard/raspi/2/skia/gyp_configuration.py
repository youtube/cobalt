# Copyright 2017 Google Inc. All Rights Reserved.
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

import logging
import os
import sys

_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_SCRIPT_DIR, '../..'))

import cobalt.tools.webdriver_benchmark_config as wb_config
# pylint: disable=g-import-not-at-top
from shared.gyp_configuration import RaspiPlatformConfig


def CreatePlatformConfig():
  try:
    return RaspiTwoSkiaPlatformConfig('raspi-2-skia')
  except RuntimeError as e:
    logging.critical(e)
    return None


class RaspiTwoSkiaPlatformConfig(RaspiPlatformConfig):

  def __init__(self, platform):
    super(RaspiTwoSkiaPlatformConfig, self).__init__(platform)

  def GetWebdriverBenchmarksTargetParams(self):
    params = super(RaspiTwoSkiaPlatformConfig,
                   self).GetWebdriverBenchmarksTargetParams()
    params.append(wb_config.DISABLE_SPLASH_SCREEN_ON_RELOADS)
    return params
