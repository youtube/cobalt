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

{
  'variables': {
    # RasPi 2 is ARMv7
    'arm_version': 7,
    'armv7': 1,
    'arm_fpu': 'neon-vfpv4',
    'arm_neon': 1,
    'arm_float_abi': 'hard',

    'compiler_flags': [
      # Optimize for Raspberry Pi 2 chips.
      '-march=armv7-a',
      '-mfloat-abi=hard',
      '-mfpu=neon-vfpv4',
      '-mcpu=cortex-a8',
      '-mtune=cortex-a8',
    ],
  },
}
