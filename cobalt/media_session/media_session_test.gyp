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
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'media_session_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'media_session_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/media_session/media_session.gyp:media_session',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'media_session_test_deploy',
      'type': 'none',
      'dependencies': [
        'media_session_test',
      ],
      'variables': {
        'executable_name': 'media_session_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ]
}


