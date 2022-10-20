# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""This file contains a mapping of platform names to paths to the platforms's
implementation. To add another platform, simply add another entry in the same
style to this dictionary.
"""

import sys

try:
  from starboard.build.internal.internal_platforms import INTERNAL_PLATFORMS
except ImportError:
  INTERNAL_PLATFORMS = {}

PLATFORMS = {
    'stub': 'starboard/stub',
    'linux-x64x11': 'starboard/linux/x64x11',
    'linux-x64x11-egl': 'starboard/linux/x64x11/egl',
    'linux-x64x11-gcc-6-3': 'starboard/linux/x64x11/gcc/6.3',
    'linux-x64x11-skia': 'starboard/linux/x64x11/skia',
    'linux-x64x11-clang-3-9': 'starboard/linux/x64x11/clang/3.9',
    'android-arm': 'starboard/android/arm',
    'android-arm64': 'starboard/android/arm64',
    'android-arm64-vulkan': 'starboard/android/arm64/vulkan',
    'android-x86': 'starboard/android/x86',
    'raspi-2': 'starboard/raspi/2',
    'raspi-2-skia': 'starboard/raspi/2/skia',
    'evergreen-x64': 'starboard/evergreen/x64',
    'evergreen-x86': 'starboard/evergreen/x86',
    'evergreen-arm-hardfp': 'starboard/evergreen/arm/hardfp',
    'evergreen-arm-softfp': 'starboard/evergreen/arm/softfp',
    'evergreen-arm64': 'starboard/evergreen/arm64',
    'win-win32': 'starboard/win/win32',
}
PLATFORMS.update(INTERNAL_PLATFORMS)

if __name__ == '__main__':
  if len(sys.argv) != 2:
    raise TypeError('Usage: {} <platform_name>'.format(sys.argv[0]))
  print(PLATFORMS[sys.argv[1]])
