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

{
  'variables': {
    # RasPi 1 is ARMv6
    'arm_version': 6,
    'armv7': 0,
    'arm_neon': 0,

    'compiler_flags': [
      # Optimize for Raspberry Pi 1 chips.
      '-march=armv6zk',
      '-mcpu=arm1176jzf-s',
      '-mfloat-abi=hard',
      '-mfpu=vfp',
    ],
  },

  'target_defaults': {
    'default_configuration': 'raspi-0_debug',
    'configurations': {
      'raspi-0_debug': {
        'inherit_from': ['debug_base'],
      },
      'raspi-0_devel': {
        'inherit_from': ['devel_base'],
      },
      'raspi-0_qa': {
        'inherit_from': ['qa_base'],
      },
      'raspi-0_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
}
