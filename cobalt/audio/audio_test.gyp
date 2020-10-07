# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'audio_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'audio_node_input_output_test.cc',
      ],
      'dependencies': [
        '<@(cobalt_platform_dependencies)',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/media/media.gyp:media',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',

        # TODO: Remove the dependency below, it works around the fact that
        #       ScriptValueFactory has non-virtual method CreatePromise().
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },

    {
      'target_name': 'audio_test_deploy',
      'type': 'none',
      'dependencies': [
        'audio_test',
      ],
      'variables': {
        'executable_name': 'audio_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
