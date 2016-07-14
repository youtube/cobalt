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

import base


class PlatformConfigStarboard(base.PlatformConfigBase):
  """Starboard platform configuration."""

  def __init__(self, platform):
    super(PlatformConfigStarboard, self).__init__(platform)

  def IsStarboard(self):
    return True

  def GetVariables(self, config):
    return {
        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,
        # Cobalt relies on the Starboard implementation for DRM on all Starboard
        # platforms.
        'use_widevine': 0,
        # Whether to build with clang's Address Sanitizer instrumentation.
        'use_asan': 0,
        # Whether to build with clang's Thread Sanitizer instrumentation.
        'use_tsan': 0,
    }
