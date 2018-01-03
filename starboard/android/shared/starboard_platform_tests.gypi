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
      'target_name': 'starboard_platform_tests',
      'type': '<(gtest_target_type)',
      'includes': [
        '<(DEPTH)/starboard/shared/starboard/media/media_tests.gypi',
      ],
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
        '<@(media_tests_sources)',
        'jni_env_ext_test.cc',
      ],
      'defines': [
        # This allows the tests to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'starboard_platform_tests_deploy',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/<(starboard_path)/starboard_platform_tests.gyp:starboard_platform_tests',
      ],
      'variables': {
        'executable_name': 'starboard_platform_tests',
      },
      'includes': [ '../../build/deploy.gypi' ],
    },
  ],
}
