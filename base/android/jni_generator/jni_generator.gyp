# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'jni_generator_py_tests',
      'type': 'none',
      'actions': [
        {
          'action_name': 'run_jni_generator_py_tests',
          'inputs': [
            'jni_generator.py',
            'jni_generator_tests.py',
            'SampleForTests.java',
            'golden_sample_for_tests_jni.h',
          ],
          'outputs': [
            '',
          ],
          'action': [
            'python', 'jni_generator_tests.py',
          ],
        },
      ],
    },
    {
      'target_name': 'jni_sample_header',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_jni_sample_header',
          'inputs': [
              'jni_generator.py',
              'SampleForTests.java',
          ],
          'outputs': [
              '<(SHARED_INTERMEDIATE_DIR)/base/jni/sample_for_tests_jni.h',
          ],
          'action': [
            'python',
            'jni_generator.py',
            '-o',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'jni_generator_tests',
      'type': 'executable',
      'dependencies': [
        '../../base.gyp:test_support_base',
        'jni_generator_py_tests',
        'jni_sample_header',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/base',
      ],
      'sources': [
        'sample_for_tests.cc',
      ],
    },
  ],
}
