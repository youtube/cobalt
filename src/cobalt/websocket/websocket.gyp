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
  'variables': {
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'websocket',
      'type': 'static_library',
      'sources': [
        'web_socket.cc',
        'web_socket.h',
        'web_socket_handshake_helper.h',
        'web_socket_handshake_helper.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'websocket_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'web_socket_test.cc',
        'web_socket_handshake_helper_test.cc',
      ],
      'dependencies': [
        'websocket',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'websocket_test_deploy',
      'type': 'none',
      'dependencies': [
        'websocket_test',
      ],
      'variables': {
        'executable_name': 'websocket_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
