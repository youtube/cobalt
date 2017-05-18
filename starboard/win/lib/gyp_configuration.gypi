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
    'javascript_engine': 'mozjs',
    'cobalt_enable_jit': 0,
  },
  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
  'target_defaults': {
    'default_configuration': 'win-lib_debug',
    'configurations': {
     'lib_base': {
       'abstract': 1,
       'msvs_settings': {
         'VCLinkerTool': {
           'SubSystem': '2', # WINDOWS
         }
       }
      },
      'win-lib_debug': {
        'inherit_from': ['msvs_debug', 'lib_base'],
      },
      'win-lib_devel': {
       'inherit_from': ['msvs_devel', 'lib_base'],
      },
      'win-lib_qa': {
        'inherit_from': ['msvs_qa', 'lib_base'],
      },
      'win-lib_gold': {
        'inherit_from': ['msvs_gold', 'lib_base'],
      },
    },  # end of configurations
  },
}
