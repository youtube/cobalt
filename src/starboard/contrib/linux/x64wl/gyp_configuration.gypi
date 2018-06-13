# Copyright 2018 Google Inc. All Rights Reserved.
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
  'target_defaults': {
    'default_configuration': 'linux-x64wl_debug',
    'configurations': {
      'linux-x64wl_debug': {
        'inherit_from': ['debug_base'],
      },
      'linux-x64wl_devel': {
        'inherit_from': ['devel_base'],
      },
      'linux-x64wl_qa': {
        'inherit_from': ['qa_base'],
      },
      'linux-x64wl_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '<(DEPTH)/starboard/linux/shared/gyp_configuration.gypi',
    '<(DEPTH)/starboard/linux/shared/compiler_flags.gypi',
  ],

  'variables': {
    'gl_type': 'system_gles2',

    'platform_libraries': [
      '-lEGL',
      '-lGLESv2',
      '-lwayland-egl',
      '-lwayland-client',
    ],
    'linker_flags': [
      '-Wl,--wrap=eglGetDisplay',
    ],
  },
}
