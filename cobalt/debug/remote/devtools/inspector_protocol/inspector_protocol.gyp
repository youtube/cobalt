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

# See Chromium: third_party/blink/renderer/core/inspector/BUILD.gn

{
  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'protocol_compatibility_check',
      'type': 'none',
      'actions': [{
        'action_name': 'check_protocol_compatibility',
        'variables': {
          'script_path': '<(DEPTH)/third_party/inspector_protocol/check_protocol_compatibility.py',
          'input_files': [
            'browser_protocol.pdl',
            'browser_protocol-1.3.json',
            '<(DEPTH)/v8/include/js_protocol.pdl',
          ],
          'stamp_file': '<(SHARED_INTERMEDIATE_DIR)/cobalt/debug/remote/inspector_protocol/browser_protocol.stamp',
        },
        'inputs': [ '<(script_path)', '<@(input_files)' ],
        'outputs': [ '<(stamp_file)' ],
        'action': [
          'python',
          '<(script_path)',
          '--stamp',
          '<(stamp_file)',
          '<@(input_files)',
        ],
      }],
    },

    {
      'target_name': 'inspector_protocol',
      'type': 'none',
      'dependencies': [
        'protocol_compatibility_check',
      ],
      'actions': [{
        'action_name': 'concatenate_protocols',
        'variables': {
          'script_path': '<(DEPTH)/third_party/inspector_protocol/concatenate_protocols.py',
          'input_files': [
            'browser_protocol.pdl',
            '<(DEPTH)/v8/include/js_protocol.pdl',
          ],
          'output_file': '<(SHARED_INTERMEDIATE_DIR)/cobalt/debug/remote/inspector_protocol/inspector_protocol.json',
        },
        'inputs': [ '<(script_path)', '<@(input_files)' ],
        'outputs': [ '<(output_file)' ],
        'action': [
          'python',
          '<(script_path)',
          '<@(input_files)',
          '<(output_file)',
        ],
      }],
    },
  ],
}
