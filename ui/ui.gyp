# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'toolkit_views2': 0,  # ui/views/ is an experimental framework on Windows.
  },
  'target_defaults': {
    'conditions': [
      ['OS=="win"',
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
    'ui_resources.gypi',
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
