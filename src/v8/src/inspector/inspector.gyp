# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'inspector.gypi',
  ],
  'targets': [
    { 'target_name': 'protocol_compatibility',
      'type': 'none',
      'toolsets': ['target'],
      'actions': [
        {
          'action_name': 'protocol_compatibility',
          'inputs': [
            '<(v8_inspector_js_protocol)',
          ],
          'outputs': [
            '<@(inspector_generated_output_root)/src/js_protocol.stamp',
          ],
          'action': [
            'python2',
            '<(inspector_protocol_path)/check_protocol_compatibility.py',
            '--stamp', '<@(_outputs)',
            '<@(_inputs)',
          ],
          'message': 'Checking inspector protocol compatibility',
        },
      ]
    },
    { 'target_name': 'protocol_generated_sources',
      'type': 'none',
      'dependencies': [ 'protocol_compatibility' ],
      'actions': [
        {
          'action_name': 'protocol_generated_sources',
          'inputs': [
            '<(v8_inspector_js_protocol)',
            '<(inspector_path)/inspector_protocol_config.json',
            '<@(inspector_protocol_files)',
          ],
          'outputs': [
            '<@(inspector_generated_sources)',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python2',
            '<(inspector_protocol_path)/code_generator.py',
            '--jinja_dir', '<(DEPTH)/third_party',
            '--output_base', '<(inspector_generated_output_root)/src/inspector',
            '--config', '<(inspector_path)/inspector_protocol_config.json',
            '--inspector_protocol_dir', 'v8/third_party/inspector_protocol',
          ],
          'message': 'Generating inspector protocol sources from protocol json',
        },
      ]
    },
  ],
}
