# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
    'arm_version': 8,
    'armv7': 0,
    'arm_thumb': 0,
    'arm_neon': 0,
    'arm_fpu': 'vfpv3-d16',

    'enable_vulkan': 1,

    'platform_libraries': [
      '-lc++_shared',
      '-lEGL_angle',
      '-lGLESv2_angle',
      '-lGLESv1_CM_angle',
      '-lfeature_support_angle',
      '-lOpenSLES',
      '-landroid',
      '-llog',
      '-lmediandk',
    ],

    'linker_flags': [
      '-Wl',
      '--verbose',
      '-Llib',
    ],
  },

  'target_defaults': {
    'default_configuration': 'android-arm64-vulkan_debug',
    'configurations': {
      'android-arm64-vulkan_debug': {
        'inherit_from': ['debug_base'],
      },
      'android-arm64-vulkan_devel': {
        'inherit_from': ['devel_base'],
      },
      'android-arm64-vulkan_qa': {
        'inherit_from': ['qa_base'],
      },
      'android-arm64-vulkan_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '<(DEPTH)/starboard/android/shared/gyp_configuration.gypi',
  ],
}
