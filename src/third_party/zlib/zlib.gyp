# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      [ 'OS == "none"', {
        # Because we have a patched zlib, we cannot use the system libz.
        # TODO(pvalchev): OpenBSD is purposefully left out, as the system
        # zlib brings up an incompatibility that breaks rendering.
        'use_system_zlib%': 1,
      }, {
        'use_system_zlib%': 0,
      }],
    ],
    'use_system_minizip%': 0,
  },
  'targets': [
    {
      'target_name': 'zlib',
      'type': 'static_library',
      'conditions': [
        ['use_system_zlib==0', {
          'sources': [
            'adler32.c',
            'compress.c',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'gzio.c',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.c',
            'inflate.h',
            'inftrees.c',
            'inftrees.h',
            'mozzconf.h',
            'trees.c',
            'trees.h',
            'uncompr.c',
            'zconf.h',
            'zlib.h',
            'zutil.c',
            'zutil.h',
          ],
          'include_dirs': [
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS!="win"', {
              'product_name': 'chrome_zlib',
            }], ['OS=="android"', {
              'toolsets': ['target', 'host'],
            }],
            ['OS=="starboard" or OS=="lb_shell"', {
              'sources!': [
                'gzio.c',
              ],
            }],
          ],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ZLIB',
            ],
          },
          'defines': [
            'USE_SYSTEM_ZLIB',
          ],
          'link_settings': {
            'libraries': [
              '-lz',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'minizip',
      'type': 'static_library',
      'conditions': [
        ['use_system_minizip==0', {
          'sources': [
            'contrib/minizip/ioapi.c',
            'contrib/minizip/ioapi.h',
            'contrib/minizip/iowin32.c',
            'contrib/minizip/iowin32.h',
            'contrib/minizip/unzip.c',
            'contrib/minizip/unzip.h',
            'contrib/minizip/zip.c',
            'contrib/minizip/zip.h',
          ],
          'include_dirs': [
            '.',
            '../..',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS!="win"', {
              'sources!': [
                'contrib/minizip/iowin32.c'
              ],
            }],
            ['OS=="android"', {
              'toolsets': ['target', 'host'],
            }],
            ['OS=="starboard" or OS=="lb_shell"', {
              # NOTE: This library is not used in Cobalt, so completely
              # disabling it to prove it. If re-enabled, will have to be ported
              # to Starboard. Alternatively, we could delete it from the repo.
              'sources/': [
                ['exclude', '.*'],
              ],
            }],
          ],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_MINIZIP',
            ],
          },
          'defines': [
            'USE_SYSTEM_MINIZIP',
          ],
          'link_settings': {
            'libraries': [
              '-lminizip',
            ],
          },
        }],
        ['OS in ["mac", "ios", "android"] or os_bsd==1 or target_arch in ["ps3", "wiiu"]', {
          # Mac, Android and the BSDs don't have fopen64, ftello64, or
          # fseeko64. We use fopen, ftell, and fseek instead on these
          # systems.
          'defines': [
            'USE_FILE32API'
          ],
        }],
        ['clang==1', {
          'xcode_settings': {
            'WARNING_CFLAGS': [
              # zlib uses `if ((a == b))` for some reason.
              '-Wno-parentheses-equality',
            ],
          },
          'cflags': [
            '-Wno-parentheses-equality',
          ],
        }],
      ],
    }
  ],
}
