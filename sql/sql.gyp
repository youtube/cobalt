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
        'init_status.h',
        'meta_table.cc',
        'meta_table.h',
        'statement.cc',
        'statement.h',
        'transaction.cc',
        'transaction.h',
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
        'transaction_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
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
  ],
}
