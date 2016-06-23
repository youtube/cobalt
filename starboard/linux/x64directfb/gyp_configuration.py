# Copyright 2015 Google Inc. All Rights Reserved.
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
"""Starboard Linux X64 DirectFB platform configuration for gyp_cobalt."""

import logging
import os
import sys

# Import the shared Linux platform configuration.
sys.path.append(os.path.realpath(os.path.join(
    os.path.dirname(__file__), os.pardir, 'shared')))
import gyp_configuration


def CreatePlatformConfig():
  try:
    return gyp_configuration.PlatformConfig('linux-x64directfb',
                                            asan_enabled_by_default=False)
  except RuntimeError as e:
    logging.critical(e)
    return None
