# Copyright 2016 Google Inc. All Rights Reserved.
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
      'target_name': 'eztime',
      'type': 'static_library',
      'sources': [
        'eztime.cc',
        'eztime.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/starboard/client_porting/icu_init/icu_init.gyp:icu_init',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
    },
    {
      'target_name': 'eztime_test',
      'type': '<(gtest_target_type)',
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
        'test_constants.h',
        'eztime_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'eztime',
      ],
    },
    {
      'target_name': 'eztime_test_deploy',
      'type': 'none',
      'dependencies': [
        'eztime_test',
      ],
      'variables': {
        'executable_name': 'eztime_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
