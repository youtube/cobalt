# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard stub platform configuration for gyp_cobalt."""

import logging

from internal.starboard.darwin.tvos.shared import gyp_configuration as shared_configuration


def CreatePlatformConfig():
  try:
    return DarwinTvOSArm64Configuration('darwin-tvos-arm64')
  except RuntimeError as e:
    logging.critical(e)
    return None


class DarwinTvOSArm64Configuration(shared_configuration.DarwinConfiguration):
  """Starboard Darwin tvOS arm64 platform configuration."""
  pass
