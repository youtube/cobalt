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
"""Starboard Raspberry Pi 0 platform configuration for gyp_cobalt."""

import logging
import os
import sys

_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_SCRIPT_DIR, '..'))

import cobalt.tools.webdriver_benchmark_config as wb_config
# pylint: disable=g-import-not-at-top
from shared.gyp_configuration import RaspiPlatformConfig


def CreatePlatformConfig():
  try:
    return RaspiZeroPlatformConfig('raspi-0')
  except RuntimeError as e:
    logging.critical(e)
    return None


class RaspiZeroPlatformConfig(RaspiPlatformConfig):

  def __init__(self, platform):
    super(RaspiZeroPlatformConfig, self).__init__(platform)

  def GetDefaultSampleSize(self):
    return wb_config.REDUCED_SIZE
