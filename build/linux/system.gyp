# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['sysroot!=""', {
        'pkg-config': './pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
      }, {
        'pkg-config': 'pkg-config'
      }]
    ],

    'linux_link_libpci%': 0,
  },
  'conditions': [
    [ 'os_posix==1 and OS!="mac"', {
      'variables': {
        # We use our own copy of libssl3, although we still need to link against
        # the rest of NSS.
        'use_system_ssl%': 0,
      },
    }, {
      'variables': {
        'use_system_ssl%': 1,
      },
    }],
    [ 'chromeos==0', {
      # Hide GTK and related dependencies for Chrome OS, so they won't get
      # added back to Chrome OS. Don't try to use GTK on Chrome OS.
      'targets': [
        {
          'target_name': 'gtk',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'variables': {
            # gtk requires gmodule, but it does not list it as a dependency
            # in some misconfigured systems.
            'gtk_packages': 'gmodule-2.0 gtk+-2.0 gthread-2.0',
          },
          'conditions': [
            ['_toolset=="target"', {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags <(gtk_packages))',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other <(gtk_packages))',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l <(gtk_packages))',
                ],
              },
            }, {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(pkg-config --cflags <(gtk_packages))',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other <(gtk_packages))',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l <(gtk_packages))',
                ],
              },
            }],
          ],
        },
        {
          'target_name': 'gtkprint',
          'type': 'none',
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
            }],
          ],
        },
        {
          'target_name': 'gdk',
          'type': 'none',
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
            }],
          ],
        },
      ],  # targets
    }],
  ],  # conditions
  'targets': [
    {
      'target_name': 'ssl',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'conditions': [
            ['use_openssl==1', {
              'dependencies': [
                '../../third_party/openssl/openssl.gyp:openssl',
              ],
            }],
            ['use_openssl==0 and use_system_ssl==0', {
              'dependencies': [
                '../../net/third_party/nss/ssl.gyp:libssl',
                '../../third_party/zlib/zlib.gyp:zlib',
              ],
              'direct_dependent_settings': {
                'include_dirs+': [
                  # We need for our local copies of the libssl3 headers to come
                  # before other includes, as we are shadowing system headers.
                  '<(DEPTH)/net/third_party/nss/ssl',
                ],
                'cflags': [
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
            }],
            ['use_openssl==0 and use_system_ssl==1', {
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
            }],
          ]
        }],
      ],
    },
    {
      'target_name': 'freetype2',
      'type': 'none',
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
        }],
      ],
    },
    {
      'target_name': 'fontconfig',
      'type': 'none',
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
        }],
      ],
    },
    {
      'target_name': 'gconf',
      'type': 'none',
      'conditions': [
        ['use_gconf==1 and _toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gconf-2.0)',
            ],
            'defines': [
              'USE_GCONF',
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
        }],
      ],
    },
    {
      'target_name': 'gio',
      'type': 'static_library',
      'conditions': [
        ['use_gio==1 and _toolset=="target"', {
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'cflags': [
            '<!@(<(pkg-config) --cflags gio-2.0)',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gio-2.0)',
            ],
            'defines': [
              'USE_GIO',
            ],
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gio-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gio-2.0)',
            ],
            'conditions': [
              ['linux_link_gsettings==0 and OS=="linux"', {
                'libraries': [
                  '-ldl',
                ],
              }],
            ],
          },
          'hard_dependency': 1,
          'actions': [
            {
              'variables': {
                'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libgio.h',
                'output_cc': '<(INTERMEDIATE_DIR)/libgio_loader.cc',
                'generator': '../../tools/generate_library_loader/generate_library_loader.py',
              },
              'action_name': 'generate_libgio_loader',
              'inputs': [
                '<(generator)',
              ],
              'outputs': [
                '<(output_h)',
                '<(output_cc)',
              ],
              'action': ['python',
                         '<(generator)',
                         '--name', 'LibGioLoader',
                         '--output-h', '<(output_h)',
                         '--output-cc', '<(output_cc)',
                         '--header', '<gio/gio.h>',
                         # TODO(phajdan.jr): This will no longer be needed
                         # after switch to Precise, http://crbug.com/158577 .
                         '--bundled-header', '"build/linux/gsettings.h"',
                         '--link-directly=<(linux_link_gsettings)',
                         'g_settings_new',
                         'g_settings_get_child',
                         'g_settings_get_string',
                         'g_settings_get_boolean',
                         'g_settings_get_int',
                         'g_settings_get_strv',
                         'g_settings_list_schemas',
              ],
              'message': 'Generating libgio library loader.',
              'process_outputs_as_sources': 1,
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'libpci',
      'type': 'static_library',
      'cflags': [
        '<!@(<(pkg-config) --cflags libpci)',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
        'conditions': [
          ['linux_link_libpci==1', {
            'link_settings': {
              'ldflags': [
                '<!@(<(pkg-config) --libs-only-L --libs-only-other libpci)',
              ],
              'libraries': [
                '<!@(<(pkg-config) --libs-only-l libpci)',
              ],
            }
          }],
        ],
      },
      'hard_dependency': 1,
      'actions': [
        {
          'variables': {
            'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libpci.h',
            'output_cc': '<(INTERMEDIATE_DIR)/libpci_loader.cc',
            'generator': '../../tools/generate_library_loader/generate_library_loader.py',
          },
          'action_name': 'generate_libpci_loader',
          'inputs': [
            '<(generator)',
          ],
          'outputs': [
            '<(output_h)',
            '<(output_cc)',
          ],
          'action': ['python',
                     '<(generator)',
                     '--name', 'LibPciLoader',
                     '--output-h', '<(output_h)',
                     '--output-cc', '<(output_cc)',
                     '--header', '<pci/pci.h>',
                     # TODO(phajdan.jr): Report problem to pciutils project
                     # and get it fixed so that we don't need --use-extern-c.
                     '--use-extern-c',
                     '--link-directly=<(linux_link_libpci)',
                     'pci_alloc',
                     'pci_init',
                     'pci_cleanup',
                     'pci_scan_bus',
                     'pci_fill_info',
                     'pci_lookup_name',
          ],
          'message': 'Generating libpci library loader.',
          'process_outputs_as_sources': 1,
        },
      ],
    },
    {
      'target_name': 'x11',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags x11)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other x11 xi)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l x11 xi)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags x11)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other x11 xi)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l x11 xi)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'xext',
      'type': 'none',
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
        }],
      ],
    },
    {
      'target_name': 'xfixes',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xfixes)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xfixes)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xfixes)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libgcrypt',
      'type': 'none',
      'conditions': [
        ['_toolset=="target" and use_cups==1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(libgcrypt-config --cflags)',
            ],
          },
          'link_settings': {
            'libraries': [
              '<!@(libgcrypt-config --libs)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'selinux',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'link_settings': {
            'libraries': [
              '-lselinux',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'gnome_keyring',
      'type': 'static_library',
      'conditions': [
        ['use_gnome_keyring==1', {
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'cflags': [
            '<!@(<(pkg-config) --cflags gnome-keyring-1)',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gnome-keyring-1)',
            ],
            'defines': [
              'USE_GNOME_KEYRING',
            ],
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'link_settings': {
            'conditions': [
              ['linux_link_gnome_keyring==1', {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other gnome-keyring-1)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l gnome-keyring-1)',
                ],
              }, { # linux_link_gnome_keyring==0
                'libraries': [
                  '-ldl',
                ],
              }],
            ],
          },
          'hard_dependency': 1,
          'actions': [
            {
              'variables': {
                'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libgnome-keyring.h',
                'output_cc': '<(INTERMEDIATE_DIR)/libgnome_keyring_loader.cc',
                'generator': '../../tools/generate_library_loader/generate_library_loader.py',
              },
              'action_name': 'generate_libgnome_keyring_loader',
              'inputs': [
                '<(generator)',
              ],
              'outputs': [
                '<(output_h)',
                '<(output_cc)',
              ],
              'action': ['python',
                         '<(generator)',
                         '--name', 'LibGnomeKeyringLoader',
                         '--output-h', '<(output_h)',
                         '--output-cc', '<(output_cc)',
                         '--header', '<gnome-keyring.h>',
                         '--link-directly=<(linux_link_gnome_keyring)',
                         'gnome_keyring_is_available',
                         'gnome_keyring_store_password',
                         'gnome_keyring_delete_password',
                         'gnome_keyring_find_itemsv',
                         'gnome_keyring_result_to_message',
              ],
              'message': 'Generating libgnome-keyring library loader.',
              'process_outputs_as_sources': 1,
            },
          ],
        }],
      ],
    },
    {
      # The unit tests use a few convenience functions from the GNOME
      # Keyring library directly. We ignore linux_link_gnome_keyring and
      # link directly in this version of the target to allow this.
      # *** Do not use this target in the main binary! ***
      'target_name': 'gnome_keyring_direct',
      'type': 'none',
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
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gnome-keyring-1)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gnome-keyring-1)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'dbus',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags dbus-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l dbus-1)',
        ],
      },
    },
    {
      'target_name': 'glib',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'variables': {
        'glib_packages': 'glib-2.0 gmodule-2.0 gobject-2.0 gthread-2.0',
      },
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l <(glib_packages))',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l <(glib_packages))',
            ],
          },
        }],
        ['chromeos==1', {
          'link_settings': {
            'libraries': [ '-lXtst' ]
          }
        }],
      ],
    },
    {
      'target_name': 'pangocairo',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags pangocairo)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other pangocairo)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l pangocairo)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags pangocairo)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other pangocairo)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l pangocairo)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libresolv',
      'type': 'none',
      'link_settings': {
        'libraries': [
          '-lresolv',
        ],
      },
    },
    {
      'target_name': 'ibus',
      'type': 'none',
      'conditions': [
        ['use_ibus==1', {
          'variables': {
            'ibus_min_version': '1.3.99.20110425',
          },
          'direct_dependent_settings': {
            'defines': ['HAVE_IBUS=1'],
            'cflags': [
              '<!@(<(pkg-config) --cflags "ibus-1.0 >= <(ibus_min_version)")',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other "ibus-1.0 >= <(ibus_min_version)")',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l "ibus-1.0 >= <(ibus_min_version)")',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'udev',
      'type': 'none',
      'conditions': [
        # libudev is not available on *BSD
        ['_toolset=="target" and os_bsd!=1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libudev)'
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libudev)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libudev)',
            ],
          },
        }],
      ],
    },
  ],
}
