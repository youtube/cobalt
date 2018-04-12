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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'websocket',
      'type': 'static_library',
      'sources': [
        'buffered_amount_tracker.cc',
        'buffered_amount_tracker.h',
        'close_event.h',
        'sec_web_socket_key.h',
        'web_socket.cc',
        'web_socket.h',
        'web_socket_event_interface.h',
        'web_socket_frame_container.cc',
        'web_socket_frame_container.h',
        'web_socket_handshake_helper.cc',
        'web_socket_handshake_helper.h',
        'web_socket_impl.cc',
        'web_socket_impl.h',
        'web_socket_message_container.cc',
        'web_socket_message_container.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'websocket_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'buffered_amount_tracker_test.cc',
        'web_socket_frame_container_test.cc',
        'web_socket_handshake_helper_test.cc',
        'web_socket_message_container_test.cc',
        'web_socket_test.cc',
      ],
      'dependencies': [
        'websocket',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',

        # TODO: Remove the dependency below, it works around the fact that
        #       ScriptValueFactory has non-virtual method CreatePromise().
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

