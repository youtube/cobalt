# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gyp file contains the platform-specific optimizations for Skia
{
  'targets': [
    {
      'target_name': 'skia_opts',
      'type': 'none',
      'includes': [
        'skia_common.gypi',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/skia/include/core',
        '<(DEPTH)/third_party/skia/include/effects',
        '<(DEPTH)/third_party/skia/src/core',
        '<(DEPTH)/third_party/skia/src/opts',
        '<(DEPTH)/third_party/skia/src/utils',
      ],
      'dependencies': [
        'skia_opts_none',
      ],
    },
    {
      'target_name': 'skia_opts_none',
      'type': 'static_library',
      'includes': [
        'skia_common.gypi',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/skia/include/core',
        '<(DEPTH)/third_party/skia/include/effects',
        '<(DEPTH)/third_party/skia/src/core',
        '<(DEPTH)/third_party/skia/src/utils',
      ],
      'sources': [
        '<(DEPTH)/third_party/skia/src/opts/SkBitmapProcState_opts_none.cpp',
        '<(DEPTH)/third_party/skia/src/opts/SkBlitMask_opts_none.cpp',
        '<(DEPTH)/third_party/skia/src/opts/SkBlitRow_opts_none.cpp',
      ],
    },
  ],
}
