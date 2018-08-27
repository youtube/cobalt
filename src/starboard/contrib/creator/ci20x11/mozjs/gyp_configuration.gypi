# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
    'gl_type': 'system_gles2',
  },
  'target_defaults': {
    'default_configuration': 'creator-ci20x11-mozjs_debug',
    'configurations': {
      'creator-ci20x11-mozjs_debug': {
        'inherit_from': ['debug_base'],
      },
      'creator-ci20x11-mozjs_devel': {
        'inherit_from': ['devel_base'],
      },
      'creator-ci20x11-mozjs_qa': {
        'inherit_from': ['qa_base'],
      },
      'creator-ci20x11-mozjs_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../libraries.gypi',
    '../../shared/compiler_flags.gypi',
    '../../shared/gyp_configuration.gypi',
  ],
}
