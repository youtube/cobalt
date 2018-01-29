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
    'angle_build_winrt': 0,
    'winrt': 0,
    'enable_d3d11_feature_level_11': 1,
    'angle_platform_windows': 1,
  },
  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
  'target_defaults': {
    'default_configuration': 'win32_debug',
    'configurations': {
      'win-win32_debug': {
        'inherit_from': ['win32_base', 'msvs_debug'],
      },
      'win-win32_devel': {
       'inherit_from': ['win32_base', 'msvs_devel'],
      },
      'win-win32_qa': {
        'inherit_from': ['win32_base', 'msvs_qa'],
      },
      'win-win32_gold': {
        'inherit_from': ['win32_base', 'msvs_gold'],
      },
    },  # end of configurations
  },
}
