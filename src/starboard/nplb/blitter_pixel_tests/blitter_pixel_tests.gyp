# Copyright 2015 Google Inc. All Rights Reserved.
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
      'target_name': 'nplb_blitter_pixel_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'fixture.cc',
        'tests.cc',
        'image.cc',
        'command_line.cc',
        'main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        # libpng is needed for the Blitter API pixel tests.
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        'nplb_blitter_pixel_tests_copy_test_data',
      ],
    },
    {
      'target_name': 'nplb_blitter_pixel_tests_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/starboard/nplb/blitter_pixel_tests/data',
        ],
        'content_test_output_subdir': 'starboard/nplb/blitter_pixel_tests',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
    {
      'target_name': 'nplb_blitter_pixel_tests_deploy',
      'type': 'none',
      'dependencies': [
        'nplb_blitter_pixel_tests',
      ],
      'variables': {
        'executable_name': 'nplb_blitter_pixel_tests',
      },
      'includes': [ '../../build/deploy.gypi' ],
    },
  ],
}
