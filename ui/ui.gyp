# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'toolkit_views2': 0,  # ui/views/ is an experimental framework on Windows.
  },
  'target_defaults': {
    'sources/': [
      ['exclude', '/(cocoa|gtk|win)/'],
      ['exclude', '_(cocoa|gtk|linux|mac|posix|win|x)\\.(cc|mm?)$'],
      ['exclude', '/(gtk|win|x11)_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['toolkit_uses_gtk == 1', {
        'sources/': [
          ['include', '/gtk/'],
          ['include', '_(gtk|linux|posix|skia|x)\\.cc$'],
          ['include', '/(gtk|x11)_[^/]*\\.cc$'],
        ],
      }],
      ['OS=="mac"', {'sources/': [
        ['include', '/cocoa/'],
        ['include', '_(cocoa|mac|posix)\\.(cc|mm?)$'],
      ]}, { # else: OS != "mac"
        'sources/': [
          ['exclude', '\\.mm?$'],
        ],
      }],
      ['OS=="win"',
        {'sources/': [
          ['include', '_(win)\\.cc$'],
          ['include', '/win/'],
          ['include', '/win_[^/]*\\.cc$'],
        ]},
        {'variables': {'toolkit_views2': 1}},
      ],
      ['toolkit_views2==0', {'sources/': [
        ['exclude', 'views/'],
      ]}],
      ['touchui==0', {'sources/': [
        ['exclude', 'event_x.cc$'],
        ['exclude', 'native_menu_x.cc$'],
        ['exclude', 'native_menu_x.h$'],
        ['exclude', 'touchui/'],
        ['exclude', '_(touch)\\.cc$'],
      ]}],
    ],
  },
  'includes': [
    'ui_base.gypi',
    'ui_gfx.gypi',
  ],
  'conditions': [
    ['toolkit_views2==1', {
      'includes': [
        'ui_views.gypi',
      ],
    }],
    ['inside_chromium_build==1', {
      'includes': [
        'ui_unittests.gypi',
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
