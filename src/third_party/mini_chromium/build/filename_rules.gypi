# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_conditions': [
    ['host_os!="mac"', {
      'sources/': [
        ['exclude', '_(cocoa|mac)(_test)?\\.(h|cc|mm?)$'],
        ['exclude', '(^|/)(cocoa|mac|mach)/'],
      ],
    }],
    ['host_os!="linux"', {
      'sources/': [
        ['exclude', '_linux(_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)linux/'],
      ],
    }],
    ['host_os!="android"', {
      'sources/': [
        ['exclude', '_android(_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)android/'],
      ],
    }],
    ['host_os=="win"', {
      'sources/': [
        ['exclude', '_posix(_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)posix/'],
      ],
    }, {
      'sources/': [
        ['exclude', '_win(_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)win/'],
      ],
    }],
  ],
}
