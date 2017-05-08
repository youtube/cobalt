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

{
  'variables': {
    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'mozjs-45',
    'cobalt_enable_jit': 0,
  },
  'target_defaults': {
    'default_configuration': 'linux-x64x11_debug',
    'configurations': {
      'linux-x64x11_debug': {
        'inherit_from': ['debug_base'],
      },
      'linux-x64x11_devel': {
        'inherit_from': ['devel_base'],
      },
      'linux-x64x11_qa': {
        'inherit_from': ['qa_base'],
      },
      'linux-x64x11_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    'libraries.gypi',
    '../shared/compiler_flags.gypi',
    '../shared/gyp_configuration.gypi',
  ],
}
