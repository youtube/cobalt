# Copyright 2014 Google Inc. All Rights Reserved.
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
      'target_name': 'browser',
      'type': 'static_library',
      'sources': [
        'web_content.cc',
        'web_content.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'browser_copy_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'browser_copy_test_data',
          'variables': {
            'test_data_files': [
              '<(DEPTH)/cobalt/browser/testdata/',
            ],
            'test_data_prefix': 'cobalt/browser',
          },
          'includes': [ '../build/copy_test_data.gypi' ],
        },
      ],
    },

    {
      'target_name': 'browser_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'web_content_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/testing/testing.gyp:browser_testing',
        'browser',
        'browser_copy_test_data',
      ],
    },

    {
      'target_name': 'browser_test_deploy',
      'type': 'none',
      'dependencies': [
        'browser_test',
      ],
      'variables': {
        'executable_name': 'browser_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

    # This target make the browser component tests visible to the toolchain.
    {
      'target_name': 'browser_component_tests',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/css/css.gyp:css_test_deploy',
        '<(DEPTH)/cobalt/browser/html/html.gyp:html_test_deploy',
      ],
    },

  ],
}
