# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'ots-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ots',
      'type': 'static_library',
      'sources': [
        '<@(ots_sources)',
      ],
      'include_dirs': [
        '<@(ots_include_dirs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(ots_include_dirs)',
        ],
      },
      'dependencies': [
        '../brotli/brotli.gyp:dec',
        '../woff2/woff2.gyp:woff2_dec',
        '../zlib/zlib.gyp:zlib',
      ],
      # Disable windows ots warnings:
      #   4334: '<<' : result of 32-bit shift implicitly converted to 64
      #       bits (was 64-bit shift intended?)
      'msvs_disabled_warnings': [ 4334 ],
    },
  ],
}
