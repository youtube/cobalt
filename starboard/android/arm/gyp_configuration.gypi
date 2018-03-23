# Copyright 2017 Google Inc. All Rights Reserved.
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
    'target_arch': 'arm',
    'arm_version': 7,
    'armv7': 1,
    'arm_thumb': 0,
    'arm_neon': 0,
    'arm_fpu': 'vfpv3-d16',
    'compiler_flags': [
      '-march=armv7-a',
    ],
    'linker_flags': [
      # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
      '-Wl,--exclude-libs,libunwind.a',
    ]
  },

  'target_defaults': {
    'default_configuration': 'android-arm_debug',
    'configurations': {
      'android-arm_debug': {
        'inherit_from': ['debug_base'],
      },
      'android-arm_devel': {
        'inherit_from': ['devel_base'],
      },
      'android-arm_qa': {
        'inherit_from': ['qa_base'],
      },
      'android-arm_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
}
