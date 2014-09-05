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
      'target_name': 'web_content',
      'type': 'static_library',
      'sources': [
        'web_content.h',
        'web_content.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'web_content_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'web_content_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'web_content',
      ],

      'actions': [
        {
          'action_name': 'copy_test_data',
          'variables': {
            'test_data_files': [
              'testdata/',
            ],
            'test_data_prefix': 'cobalt/browser',
          },
          'includes': [ '../build/copy_test_data.gypi' ],
        },
      ],
    },

    {
      'target_name': 'web_content_test_deploy',
      'type': 'none',
      'dependencies': [
        'web_content_test',
      ],
      'variables': {
        'executable_name': 'web_content_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

  ],
}
