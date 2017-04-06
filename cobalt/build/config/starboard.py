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
"""Starboard base platform configuration for gyp_cobalt."""

import logging
import os

from config.base import Configs
from config.base import PlatformConfigBase


class PlatformConfigStarboard(PlatformConfigBase):
  """Starboard platform configuration."""

  def __init__(self, platform, asan_enabled_by_default=False):
    super(PlatformConfigStarboard, self).__init__(platform)
    self.asan_default = 1 if asan_enabled_by_default else 0

  def IsStarboard(self):
    return True

  def GetVariables(self, config, use_clang=0):
    use_asan = 0
    use_tsan = 0
    vr_enabled = 0
    if use_clang:
      use_tsan = int(os.environ.get('USE_TSAN', 0))
      # Enable ASAN by default for debug and devel builds only if USE_TSAN was
      # not set to 1 in the environment.
      use_asan_default = self.asan_default if not use_tsan and config in (
          Configs.DEBUG, Configs.DEVEL) else 0
      use_asan = int(os.environ.get('USE_ASAN', use_asan_default))

      # Set environmental variable to enable_vr: 'USE_VR'
      # Terminal: `export {varname}={value}`
      # Note: must also edit gyp_configuration.gypi per internal instructions.
      vr_on_by_default = 0
      vr_enabled = int(os.environ.get('USE_VR', vr_on_by_default))

    if use_asan == 1 and use_tsan == 1:
      raise RuntimeError('ASAN and TSAN are mutually exclusive')

    if use_asan:
      logging.info('Using ASan Address Sanitizer')
    if use_tsan:
      logging.info('Using TSan Thread Sanitizer')

    variables = {
        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,
        'clang': use_clang,
        # Cobalt relies on the Starboard implementation for DRM on all Starboard
        # platforms.
        'use_widevine': 0,
        # Whether to build with clang's Address Sanitizer instrumentation.
        'use_asan': use_asan,
        # Whether to build with clang's Thread Sanitizer instrumentation.
        'use_tsan': use_tsan,
        # Whether to enable VR.
        'enable_vr': vr_enabled,
    }
    return variables
