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
# See the License for the specif

{
  'targets': [
    {
      'target_name': 'accessibility_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'screen_reader_tests.cc',
        'internal/live_region_test.cc',
        'internal/text_alternative_helper_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/accessibility/accessibility.gyp:accessibility',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'accessibility_test_data',
      ],
    },
    {
      'target_name': 'accessibility_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/accessibility/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/accessibility/testdata/',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
    {
      'target_name': 'accessibility_test_deploy',
      'type': 'none',
      'dependencies': [
        'accessibility_test',
      ],
      'variables': {
        'executable_name': 'accessibility_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ]
}
