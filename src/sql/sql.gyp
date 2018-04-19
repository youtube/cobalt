# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'sql',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'defines': [ 'SQL_IMPLEMENTATION' ],
      'sources': [
        'connection.cc',
        'connection.h',
        'diagnostic_error_delegate.h',
        'error_delegate_util.cc',
        'error_delegate_util.h',
        'init_status.h',
        'meta_table.cc',
        'meta_table.h',
        'statement.cc',
        'statement.h',
        'transaction.cc',
        'transaction.h',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'dependencies': [
            '<(DEPTH)/starboard/starboard.gyp:starboard',
          ],
        }],
      ],
    },
    {
      'target_name': 'sql_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'sql',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'run_all_unittests.cc',
        'connection_unittest.cc',
        'sqlite_features_unittest.cc',
        'statement_unittest.cc',
        'test_vfs.h',
        'test_vfs.cc',
        'transaction_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # Special target to wrap a gtest_target_type==shared_library
    # sql_unittests into an android apk for execution.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'sql_unittests_apk',
          'type': 'none',
          'dependencies': [
            'sql_unittests',
          ],
          'variables': {
            'test_suite_name': 'sql_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)sql_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
    ['cobalt==1', {
      'targets': [
        {
          'target_name': 'sql_unittests_deploy',
          'type': 'none',
          'dependencies': [
            'sql_unittests',
          ],
          'variables': {
            'executable_name': 'sql_unittests',
          },
          'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
        },
      ],
    }],
  ],
}
