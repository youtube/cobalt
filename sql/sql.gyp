# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'sql',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
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
      'type': 'executable',
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
        ['os_posix==1 and OS!="mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
