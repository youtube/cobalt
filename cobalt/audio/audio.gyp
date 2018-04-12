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
      'target_name': 'audio',
      'type': 'static_library',
      'sources': [
        'async_audio_decoder.cc',
        'async_audio_decoder.h',
        'audio_buffer.cc',
        'audio_buffer.h',
        'audio_buffer_source_node.cc',
        'audio_buffer_source_node.h',
        'audio_context.cc',
        'audio_context.h',
        'audio_destination_node.cc',
        'audio_destination_node.h',
        'audio_device.cc',
        'audio_device.h',
        'audio_file_reader.cc',
        'audio_file_reader.h',
        'audio_file_reader_wav.cc',
        'audio_file_reader_wav.h',
        'audio_helpers.h',
        'audio_node.cc',
        'audio_node.h',
        'audio_node_input.cc',
        'audio_node_input.h',
        'audio_node_output.cc',
        'audio_node_output.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
      ],
      'export_dependent_settings': [
        # Additionally, ensure that the include directories for generated
        # headers are put on the include directories for targets that depend
        # on this one.
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ]
    },

    {
      'target_name': 'audio_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'audio_node_input_output_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/audio/audio.gyp:audio',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',

        # TODO: Remove the dependency below, it works around the fact that
        #       ScriptValueFactory has non-virtual method CreatePromise().
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
      ],
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
