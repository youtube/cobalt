# Copyright 2014 Google Inc. All Rights Reserved.
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

# Platform specific configuration for Linux on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#

{
  'variables': {
    'platform_libraries': [
      '-ldirectfb',
      '-ldirect',
    ],

    'gl_type': 'none',

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'mozjs',
    'cobalt_enable_jit': 1,
  },

  'target_defaults': {
    'default_configuration': 'linux-x64directfb_debug',
    'configurations': {
      'linux-x64directfb_debug': {
        'inherit_from': ['debug_base'],
      },
      'linux-x64directfb_devel': {
        'inherit_from': ['devel_base'],
      },
      'linux-x64directfb_qa': {
        'inherit_from': ['qa_base'],
      },
      'linux-x64directfb_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../shared/compiler_flags.gypi',
    '../shared/gyp_configuration.gypi',
  ],
}
