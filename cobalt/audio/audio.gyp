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
      'target_name': 'audio',
      'type': 'static_library',
      'sources': [
        'audio_buffer.cc',
        'audio_buffer.h',
        'audio_buffer_source_node.cc',
        'audio_buffer_source_node.h',
        'audio_context.cc',
        'audio_context.h',
        'audio_destination_node.cc',
        'audio_destination_node.h',
        'audio_node.cc',
        'audio_node.h',
        'audio_node_input.cc',
        'audio_node_input.h',
        'audio_node_output.cc',
        'audio_node_output.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
      ],
    },
  ],
}
