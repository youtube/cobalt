# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['sysroot!=""', {
      'variables': {
        'pkg-config': './pkg-config-wrapper "<(sysroot)"',
      },
    }, {
      'variables': {
        'pkg-config': 'pkg-config'
      },
    }],
    [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
      'variables': {
        # We use our own copy of libssl, although we still need to link against
        # the rest of NSS.
        'use_system_ssl%': 0,
      },
    }, {  # OS!="linux"
      'variables': {
        'use_system_ssl%': 1,
      },
    }],
  ],


  'targets': [
    {
      'target_name': 'gtk',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gtk+-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gtk+-2.0 gthread-2.0)',
            ],
          },
      }]]
    },
    {
      'target_name': 'gtkprint',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gtk+-unix-print-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gtk+-unix-print-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gtk+-unix-print-2.0)',
            ],
          },
      }]]
    },
    {
      'target_name': 'nss',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'conditions': [
            ['use_system_ssl==0', {
              'dependencies': [
                '../../net/third_party/nss/ssl.gyp:ssl',
                '../../third_party/zlib/zlib.gyp:zlib',
              ],
              'direct_dependent_settings': {
                'cflags': [
                  # We need for our local copies of the libssl headers to come
                  # first, otherwise the code will build, but will fallback to
                  # the set of features advertised in the system headers.
                  # Unfortunately, there's no include path that we can filter
                  # out of $(pkg-config --cflags nss) and GYP include paths
                  # come after cflags on the command line. So we have these
                  # bodges:
                  '-I../net/third_party/nss/ssl',               # for scons
                  '-Inet/third_party/nss/ssl',                  # for make
                  '-IWebKit/chromium/net/third_party/nss/ssl',  # for make in webkit
                  '<!@(<(pkg-config) --cflags nss)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss | sed -e "s/-lssl3//")',
                ],
              },
          }, {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags nss)',
                ],
                'defines': [
                  'USE_SYSTEM_SSL',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss)',
                ],
              },
          }]]
        }],
      ],
    },
    {
      'target_name': 'freetype2',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags freetype2)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other freetype2)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l freetype2)',
            ],
          },
      }]]
    },
    {
      'target_name': 'fontconfig',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags fontconfig)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other fontconfig)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l fontconfig)',
            ],
          },
      }]]
    },
    {
      'target_name': 'gdk',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gdk-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gdk-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gdk-2.0)',
            ],
          },
      }]]
    },
    {
      'target_name': 'gconf',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gconf-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gconf-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gconf-2.0)',
            ],
          },
      }]]
    },
    {
      'target_name': 'x11',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags x11)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other x11)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l x11)',
            ],
          },
      }]]
    },
    {
      'target_name': 'xext',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xext)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xext)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xext)',
            ],
          },
      }]]
    },
    {
      'target_name': 'selinux',
      'type': 'settings',
      'conditions': [
        ['_toolset=="target"', {
          'link_settings': {
            'libraries': [
              '-lselinux',
            ],
          },
      }]]
    },
    {
      'target_name': 'gnome-keyring',
      'type': 'settings',
      'conditions': [
        ['use_gnome_keyring==1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gnome-keyring-1)',
            ],
            'defines': [
              'USE_GNOME_KEYRING',
            ],
            'conditions': [
              ['linux_link_gnome_keyring==0', {
                'defines': ['DLOPEN_GNOME_KEYRING'],
              }],
            ],
          },
          'conditions': [
            ['linux_link_gnome_keyring!=0', {
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other gnome-keyring-1)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l gnome-keyring-1)',
                ],
              },
            }, {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'dbus-glib',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags dbus-glib-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-glib-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l dbus-glib-1)',
        ],
      },
    },
    {
      'target_name': 'libresolv',
      'type': 'settings',
      'link_settings': {
        'libraries': [
          '-lresolv',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
