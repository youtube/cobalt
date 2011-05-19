# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
          'type': 'static_library',
          'msvs_guid': '6EAD4A4B-2BBC-4974-8E45-BB5C16CC2AC9',
          'sources': [
            'amalgamation/sqlite3.h',
            'amalgamation/sqlite3.c',
            # fts2.c currently has a lot of conflicts when added to
            # the amalgamation.  It is probably not worth fixing that.
            'src/ext/fts2/fts2.c',
            'src/ext/fts2/fts2.h',
            'src/ext/fts2/fts2_hash.c',
            'src/ext/fts2/fts2_hash.h',
            'src/ext/fts2/fts2_icu.c',
            'src/ext/fts2/fts2_porter.c',
            'src/ext/fts2/fts2_tokenizer.c',
            'src/ext/fts2/fts2_tokenizer.h',
            'src/ext/fts2/fts2_tokenizer1.c',
          ],

          # TODO(shess): Previously fts1 and rtree files were
          # explicitly excluded from the build.  Make sure they are
          # logically still excluded.

          # TODO(shess): Should all of the sources be listed and then
          # excluded?  For editing purposes?

          'include_dirs': [
            'amalgamation',
            # Needed for fts2 to build.
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
