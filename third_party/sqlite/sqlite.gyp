# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_sqlite%': 0,
    'required_sqlite_version': '3.6.1',
  },
  'target_defaults': {
    'defines': [
      'SQLITE_CORE',
      'SQLITE_ENABLE_BROKEN_FTS2',
      'SQLITE_ENABLE_FTS2',
      'SQLITE_ENABLE_FTS3',
      'SQLITE_ENABLE_ICU',
      'SQLITE_ENABLE_MEMORY_MANAGEMENT',
      'SQLITE_SECURE_DELETE',
      'THREADSAFE',
      '_HAS_EXCEPTIONS=0',
    ],
  },
  'targets': [
    {
      'target_name': 'sqlite',
      'conditions': [
        [ 'chromeos==1' , {
            'defines': [
                # Despite obvious warnings about not using this flag
                # in deployment, we are turning off sync in ChromeOS
                # and relying on the underlying journaling filesystem
                # to do error recovery properly.  It's much faster.
                'SQLITE_NO_SYNC',
                ],
          },
        ],
        ['OS=="linux" and not use_system_sqlite', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
        ['(OS=="linux" or OS=="freebsd" or OS=="openbsd") and use_system_sqlite', {
          'type': 'settings',
          'direct_dependent_settings': {
            'cflags': [
              # This next command produces no output but it it will fail (and
              # cause GYP to fail) if we don't have a recent enough version of
              # sqlite.
              '<!@(pkg-config --atleast-version=<(required_sqlite_version) sqlite3)',

              '<!@(pkg-config --cflags sqlite3)',
            ],
            'defines': [
              'USE_SYSTEM_SQLITE',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other sqlite3)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l sqlite3)',
            ],
          },
        }, { # else: OS != "linux" or ! use_system_sqlite
          'product_name': 'sqlite3',
          'type': '<(library)',
          'msvs_guid': '6EAD4A4B-2BBC-4974-8E45-BB5C16CC2AC9',
          'sources': [
            # This list contains all .h, .c, and .cc files in the directories
            # ext, preprocessed, and src, with the exception of src/lempar.c,
            # src/shell*, and src/test*.  Exclusions are applied below in the
            # sources/ and sources! sections.
            'preprocessed/keywordhash.h',
            'preprocessed/opcodes.c',
            'preprocessed/opcodes.h',
            'preprocessed/parse.c',
            'preprocessed/parse.h',
            'preprocessed/sqlite3.h',
            'src/ext/async/sqlite3async.c',
            'src/ext/async/sqlite3async.h',
            'src/ext/fts1/ft_hash.c',
            'src/ext/fts1/ft_hash.h',
            'src/ext/fts1/fts1.c',
            'src/ext/fts1/fts1.h',
            'src/ext/fts1/fts1_hash.c',
            'src/ext/fts1/fts1_hash.h',
            'src/ext/fts1/fts1_porter.c',
            'src/ext/fts1/fts1_tokenizer.h',
            'src/ext/fts1/fts1_tokenizer1.c',
            'src/ext/fts1/fulltext.c',
            'src/ext/fts1/fulltext.h',
            'src/ext/fts1/simple_tokenizer.c',
            'src/ext/fts1/tokenizer.h',
            'src/ext/fts2/fts2.c',
            'src/ext/fts2/fts2.h',
            'src/ext/fts2/fts2_hash.c',
            'src/ext/fts2/fts2_hash.h',
            'src/ext/fts2/fts2_icu.c',
            'src/ext/fts2/fts2_porter.c',
            'src/ext/fts2/fts2_tokenizer.c',
            'src/ext/fts2/fts2_tokenizer.h',
            'src/ext/fts2/fts2_tokenizer1.c',
            'src/ext/fts3/fts3.c',
            'src/ext/fts3/fts3.h',
            'src/ext/fts3/fts3_expr.c',
            'src/ext/fts3/fts3_expr.h',
            'src/ext/fts3/fts3_hash.c',
            'src/ext/fts3/fts3_hash.h',
            'src/ext/fts3/fts3_icu.c',
            'src/ext/fts3/fts3_porter.c',
            'src/ext/fts3/fts3_tokenizer.c',
            'src/ext/fts3/fts3_tokenizer.h',
            'src/ext/fts3/fts3_tokenizer1.c',
            'src/ext/icu/icu.c',
            'src/ext/icu/sqliteicu.h',
            'src/ext/rtree/rtree.c',
            'src/ext/rtree/rtree.h',
            'src/src/alter.c',
            'src/src/analyze.c',
            'src/src/attach.c',
            'src/src/auth.c',
            'src/src/backup.c',
            'src/src/bitvec.c',
            'src/src/btmutex.c',
            'src/src/btree.c',
            'src/src/btree.h',
            'src/src/btreeInt.h',
            'src/src/build.c',
            'src/src/callback.c',
            'src/src/complete.c',
            'src/src/date.c',
            'src/src/delete.c',
            'src/src/expr.c',
            'src/src/fault.c',
            'src/src/func.c',
            'src/src/global.c',
            'src/src/hash.c',
            'src/src/hash.h',
            'src/src/hwtime.h',
            'src/src/insert.c',
            'src/src/journal.c',
            'src/src/legacy.c',
            'src/src/loadext.c',
            'src/src/main.c',
            'src/src/malloc.c',
            'src/src/mem0.c',
            'src/src/mem1.c',
            'src/src/mem2.c',
            'src/src/mem3.c',
            'src/src/mem5.c',
            'src/src/memjournal.c',
            'src/src/mutex.c',
            'src/src/mutex.h',
            'src/src/mutex_noop.c',
            'src/src/mutex_os2.c',
            'src/src/mutex_unix.c',
            'src/src/mutex_w32.c',
            'src/src/notify.c',
            'src/src/os.c',
            'src/src/os.h',
            'src/src/os_common.h',
            'src/src/os_os2.c',
            'src/src/os_symbian.cc',
            'src/src/os_unix.c',
            'src/src/os_win.c',
            'src/src/pager.c',
            'src/src/pager.h',
            'src/src/pcache.c',
            'src/src/pcache.h',
            'src/src/pcache1.c',
            'src/src/pragma.c',
            'src/src/prepare.c',
            'src/src/printf.c',
            'src/src/random.c',
            'src/src/resolve.c',
            'src/src/rowset.c',
            'src/src/select.c',
            'src/src/sqlite3ext.h',
            'src/src/sqliteInt.h',
            'src/src/sqliteLimit.h',
            'src/src/status.c',
            'src/src/table.c',
            'src/src/tclsqlite.c',
            'src/src/tokenize.c',
            'src/src/trigger.c',
            'src/src/update.c',
            'src/src/utf.c',
            'src/src/util.c',
            'src/src/vacuum.c',
            'src/src/vdbe.c',
            'src/src/vdbe.h',
            'src/src/vdbeInt.h',
            'src/src/vdbeapi.c',
            'src/src/vdbeaux.c',
            'src/src/vdbeblob.c',
            'src/src/vdbemem.c',
            'src/src/vtab.c',
            'src/src/walker.c',
            'src/src/where.c',
          ],
          'sources/': [
            ['exclude', '^src/ext/(fts1|rtree)/'],
            ['exclude', '(symbian|os2|noop)\\.cc?$'],
          ],
          'sources!': [
            'src/src/journal.c',
            'src/src/tclsqlite.c',
          ],
          'include_dirs': [
            'preprocessed',
            'src/ext/icu',
            'src/src',
          ],
          'dependencies': [
            '../icu/icu.gyp:icui18n',
            '../icu/icu.gyp:icuuc',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
              '../..',
            ],
          },
          'msvs_disabled_warnings': [
            4018, 4244,
          ],
          'conditions': [
            ['OS=="win"', {
              'sources/': [['exclude', '_unix\\.cc?$']],
            }, {  # else: OS!="win"
              'sources/': [['exclude', '_(w32|win)\\.cc?$']],
            }],
            ['OS=="linux"', {
              'cflags': [
                # SQLite doesn't believe in compiler warnings,
                # preferring testing.
                #   http://www.sqlite.org/faq.html#q17
                '-Wno-int-to-pointer-cast',
                '-Wno-pointer-to-int-cast',
              ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['(OS=="linux" or OS=="freebsd" or OS=="openbsd") and not use_system_sqlite', {
      'targets': [
        {
          'target_name': 'sqlite_shell',
          'type': 'executable',
          'dependencies': [
            '../icu/icu.gyp:icuuc',
            'sqlite',
          ],
          'sources': [
            'src/src/shell.c',
            'src/src/shell_icu_linux.c',
          ],
          'link_settings': {
            'link_languages': ['c++'],
          },
        },
      ],
    },]
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
