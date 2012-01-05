# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      # __LB_PS3__FIX_ME__ remove our dependency on libpng and revert
      # this file to upstream!
      [ 'os_posix == 1 and OS != "mac" and OS != "lb_shell"', {
        # Link to system .so since we already use it due to GTK.
        'use_system_libpng%': 1,
      }, {  # os_posix != 1 or OS == "mac"
        'use_system_libpng%': 0,
      }],
    ],
  },
  'conditions': [
    ['use_system_libpng==0', {
      'targets': [
        {
          'target_name': 'libpng',
          'type': '<(component)',
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
          ],
          'defines': [
            'CHROME_PNG_WRITE_SUPPORT',
            'PNG_USER_CONFIG',
          ],
          'sources': [
            'png.c',
            'png.h',
            'pngconf.h',
            'pngerror.c',
            'pnggccrd.c',
            'pngget.c',
            'pngmem.c',
            'pngpread.c',
            'pngread.c',
            'pngrio.c',
            'pngrtran.c',
            'pngrutil.c',
            'pngset.c',
            'pngtrans.c',
            'pngusr.h',
            'pngvcrd.c',
            'pngwio.c',
            'pngwrite.c',
            'pngwtran.c',
            'pngwutil.c',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
            'defines': [
              'CHROME_PNG_WRITE_SUPPORT',
              'PNG_USER_CONFIG',
            ],
          },
          'export_dependent_settings': [
            '../zlib/zlib.gyp:zlib',
          ],
          'conditions': [
            ['OS!="win"', {'product_name': 'png'}],
            ['OS=="win" and component=="shared_library"', {
              'defines': [
                'PNG_BUILD_DLL',
                'PNG_NO_MODULEDEF',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'PNG_USE_DLL',
                ],
              },          
            }],
            ['OS=="lb_shell"', {
              # __LB_SHELL__FIX_ME__: we want to go to using the libpng code on
              # the console.  For now we aren't picking up the correct include
              # dirs for zlib
              'include_dirs': [
                '../zlib'
              ]
            }],
          ],
        },
      ]
    }, {
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../build/linux/pkg-config-wrapper "<(sysroot)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
      'targets': [
        {
          'target_name': 'libpng',
          'type': 'settings',
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libpng)',
            ],
            'defines': [
              'USE_SYSTEM_LIBPNG',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libpng)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libpng)',
            ],
          },
        },
      ],
    }],
  ],
}
