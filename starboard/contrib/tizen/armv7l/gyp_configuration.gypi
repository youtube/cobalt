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
{
  'variables': {
    'enable_map_to_mesh%': 1,
    'target_arch': 'arm',
    'gl_type%': 'system_gles2',
    'arm_neon': 1,
    'cobalt_platform_dependencies': [
      # GL Linux makes some GL calls within decode_target_internal.cc.
      '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
    ],
  },
  'target_defaults': {
    'default_configuration': 'tizen-armv7l_debug',
    'configurations': {
      'tizen-armv7l_debug': {
        'inherit_from': ['debug_base'],
      },
      'tizen-armv7l_devel': {
        'inherit_from': ['devel_base'],
      },
      'tizen-armv7l_qa': {
        'inherit_from': ['qa_base'],
      },
      'tizen-armv7l_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
    # for libvpx arm arch support
    'cflags': [
      '-mfpu=neon-vfpv4',
      '-mfloat-abi=softfp',
    ],
  },

  'includes': [
    'libraries.gypi',
    '../shared/gyp_configuration.gypi',
  ],
  # with flto flag, we get below error
  # plugin needed to handle lto object
  # so remove flto flag
  'compiler_flags_gold!': [
    '-flto',
  ],
  'linker_flags_gold!': [
    '-flto',
  ],
}
