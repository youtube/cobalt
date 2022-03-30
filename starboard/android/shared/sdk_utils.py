# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Utilities to use the toolchain from the Android NDK."""

import os
from starboard.tools import build

# Which version of the Android NDK and CMake to install and build with.
# Note that build.gradle parses these out of this file too.
_NDK_VERSION = '21.1.6352462'
_CMAKE_VERSION = '3.10.2.4988404'

_STARBOARD_TOOLCHAINS_DIR = build.GetToolchainsDir()

# The path to the Android SDK, if placed inside of starboard-toolchains.
_STARBOARD_TOOLCHAINS_SDK_DIR = os.path.join(_STARBOARD_TOOLCHAINS_DIR,
                                             'AndroidSdk')

_ANDROID_HOME = os.environ.get('ANDROID_HOME')
if _ANDROID_HOME:
  SDK_PATH = _ANDROID_HOME
else:
  SDK_PATH = _STARBOARD_TOOLCHAINS_SDK_DIR
