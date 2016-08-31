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
      'target_name': 'poem',
      'type': 'static_library',
      'sources': [
        'eztime_poem.h',
        'stdio_poem.h',
        'stdlib_poem.h',
        'string_poem.h',
        'strings_poem.h',
        'wchar_poem.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/client_porting/eztime/eztime.gyp:eztime',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
    {
      'target_name': 'poem_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'abs_tests.cc',
        'include_all.c',
        'main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
  ],
}
