# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'media_capture_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'get_user_media_test.cc',
        'media_recorder_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom/testing/dom_testing.gyp:dom_testing',
        '<(DEPTH)/cobalt/media_capture/media_capture.gyp:media_capture',

        # TODO: Remove the dependency below, it works around the fact that
        #       ScriptValueFactory has non-virtual method CreatePromise().
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        # For Fake Microphone.
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',

        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },

    {
      'target_name': 'media_capture_test_deploy',
      'type': 'none',
      'dependencies': [
        'media_capture_test',
      ],
      'variables': {
        'executable_name': 'media_capture_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
