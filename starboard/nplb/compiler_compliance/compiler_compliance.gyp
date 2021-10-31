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
  'targets': [
    {
      'target_name': 'cpp14_supported',
      'type': 'static_library',
      'cflags_cc': [
        '-std=c++14',
      ],
      'sources': [
        'cpp14_constexpr.cc',
        'cpp14_initialization.cc',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
    {
      'target_name': 'cpp17_supported',
      'type': 'static_library',
      'cflags_cc': [
        '-std=c++17',
      ],
      'sources': [
        'cpp17_support.cc',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
  ],
}
