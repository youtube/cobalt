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
        '../app/sql/connection.cc',
        '../app/sql/connection.h',
        '../app/sql/diagnostic_error_delegate.h',
        '../app/sql/init_status.h',
        '../app/sql/meta_table.cc',
        '../app/sql/meta_table.h',
        '../app/sql/statement.cc',
        '../app/sql/statement.h',
        '../app/sql/transaction.cc',
        '../app/sql/transaction.h',
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
        '../app/run_all_unittests.cc',
        '../app/sql/connection_unittest.cc',
        '../app/sql/sqlite_features_unittest.cc',
        '../app/sql/statement_unittest.cc',
        '../app/sql/transaction_unittest.cc',
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

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
