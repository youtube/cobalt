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
    'optimize_target_for_speed': 1,
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
        '<(DEPTH)/cobalt/media/media.gyp:media',
      ],
      'export_dependent_settings': [
        # Additionally, ensure that the include directories for generated
        # headers are put on the include directories for targets that depend
        # on this one.
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ]
    },
  ],
}
