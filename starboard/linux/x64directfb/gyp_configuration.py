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
"""Starboard Linux X86 DirectFB platform configuration for gyp_cobalt."""

import logging

# Import the original Linux x86 platform configuration so that we can reuse
# its configuration details since the only difference here is the use of
# DirectFB.
import config.starboard_linux


def CreatePlatformConfig():
  try:
    return config.starboard_linux.PlatformConfig('linux-x64directfb')
  except RuntimeError as e:
    logging.critical(e)
    return None
