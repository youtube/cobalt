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
      'target_name': 'layout_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'layout_tests.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        # Depend on browser_copy_test_data so that we ensure external tests
        # files from browser that we link to are available.
        '<(DEPTH)/cobalt/browser/browser.gyp:browser_copy_test_data',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:render_tree_pixel_tester',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'actions': [
        {
          'action_name': 'layout_tests_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/layout_tests/testdata/',
            ],
            'output_dir': 'cobalt/layout_tests',
          },
          'includes': ['../build/copy_data.gypi'],
        }
      ],
    },

    {
      'target_name': 'layout_tests_deploy',
      'type': 'none',
      'dependencies': [
        'layout_tests',
      ],
      'variables': {
        'executable_name': 'layout_tests',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
