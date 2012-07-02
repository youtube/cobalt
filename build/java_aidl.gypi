# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java aidl files in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'aidl_aidl-file-name',
#   'type': 'none',
#   'variables': {
#     'aidl_interface_file': 'path/to/aidl/interface/file/aidl-file',
#   },
#   'sources': {
#     'path/to/aidl/source/file/1.aidl',
#     'path/to/aidl/source/file/2.aidl',
#     ...
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
#
# Finally, the generated java file will be in the following directory:
#   <(PRODUCT_DIR)/lib.java/

{
  'rules': [
    {
      'rule_name': 'compile_aidl',
      'extension': 'aidl',
      'inputs': [
        '<(android_sdk)/framework.aidl',
        '<(aidl_interface_file)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/lib.java/<(RULE_INPUT_ROOT).java',
      ],
      'action': [
        'aidl',
        '-p<(android_sdk)/framework.aidl',
        '-p<(aidl_interface_file)',
        '-o<(PRODUCT_DIR)/lib.java/',
        '<(RULE_INPUT_PATH)',
      ],
    },
  ],
}
