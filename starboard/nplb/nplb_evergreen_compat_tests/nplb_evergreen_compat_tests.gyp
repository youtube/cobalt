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
      'target_name': 'nplb_evergreen_compat_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'checks.h',
        'crashpad_config_test.cc',
        'executable_memory_test.cc',
        'fonts_test.cc',
        'sabi_test.cc',
        '<(DEPTH)/starboard/common/test_main.cc',
      ],
      'conditions': [
        ['sb_evergreen_compatible_lite!=1', {
          'sources': [
            'icu_test.cc',
            'max_file_name_test.cc',
            'storage_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'nplb_evergreen_compat_tests_deploy',
      'type': 'none',
      'dependencies': [
        'nplb_evergreen_compat_tests',
      ],
      'variables': {
        'executable_name': 'nplb_evergreen_compat_tests',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
