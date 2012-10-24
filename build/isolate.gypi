# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to "build" .isolate files into a .isolated file.
#
# To use this, create a gyp target with the following form:
# 'conditions': [
#   ['test_isolation_mode != "noop"', {
#     'targets': [
#       {
#         'target_name': 'foo_test_run',
#         'type': 'none',
#         'dependencies': [
#           'foo_test',
#         ],
#         'includes': [
#           '../build/isolate.gypi',
#           'foo_test.isolate',
#         ],
#         'sources': [
#           'foo_test.isolate',
#         ],
#       },
#     ],
#   }],
# ],
#
# Note: foo_test.isolate is included and a source file. It is an inherent
# property of the .isolate format. This permits to define GYP variables but is
# a stricter format than GYP so isolate.py can read it.
#
# The generated .isolated file will be:
#   <(PRODUCT_DIR)/foo_test.isolated

{
  'rules': [
    {
      'rule_name': 'isolate',
      'extension': 'isolate',
      'inputs': [
        '<@(isolate_dependency_tracked)',
        '<(RULE_INPUT_PATH)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).isolated',
      ],
      'action': [
        'python',
        '<(DEPTH)/tools/swarm_client/isolate.py',
        '<(test_isolation_mode)',
        '--outdir', '<(test_isolation_outdir)',
        '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',
        '--variable', 'OS', '<(OS)',
        '--result', '<@(_outputs)',
        '--isolate', '<(RULE_INPUT_PATH)',
      ],
      'msvs_cygwin_shell': 0,
    },
  ],
}
