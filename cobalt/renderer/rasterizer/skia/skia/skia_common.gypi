# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This gypi file handles the removal of platform-specific files from the
# Skia build.
{
  'include_dirs': [
    '<(DEPTH)/third_party/skia/include/c',
    '<(DEPTH)/third_party/skia/include/codec',
    '<(DEPTH)/third_party/skia/include/core',
    '<(DEPTH)/third_party/skia/include/effects',
    '<(DEPTH)/third_party/skia/include/encode',
    '<(DEPTH)/third_party/skia/include/gpu',
    '<(DEPTH)/third_party/skia/include/images',
    '<(DEPTH)/third_party/skia/include/lazy',
    '<(DEPTH)/third_party/skia/include/pathops',
    '<(DEPTH)/third_party/skia/include/pipe',
    '<(DEPTH)/third_party/skia/include/ports',
    '<(DEPTH)/third_party/skia/include/private',
    '<(DEPTH)/third_party/skia/include/record',
    '<(DEPTH)/third_party/skia/include/utils',
    '<(DEPTH)/third_party/skia/src/codec',
    '<(DEPTH)/third_party/skia/src/core',
    '<(DEPTH)/third_party/skia/src/gpu',
    '<(DEPTH)/third_party/skia/src/opts',
    '<(DEPTH)/third_party/skia/src/image',
    '<(DEPTH)/third_party/skia/src/ports',
    '<(DEPTH)/third_party/skia/src/sfnt',
    '<(DEPTH)/third_party/skia/src/sksl',
    '<(DEPTH)/third_party/skia/src/shaders',
    '<(DEPTH)/third_party/skia/src/utils',
    '<(DEPTH)/third_party/skia/src/lazy',
    '<(DEPTH)/third_party/skia/third_party/gif',
    'config',
  ],

  'variables': {
    'skia_support_pdf': '0',
    'skia_zlib_static': '1',

    # These two set the paths so we can include skia/gyp/core.gypi
    'skia_src_path': '<(DEPTH)/third_party/skia/src',
    'skia_include_path': '<(DEPTH)/third_party/skia/include',

    # This list will contain all defines that also need to be exported to
    # dependent components.
    'skia_export_defines': [],

    # The |default_font_cache_limit| specifies the max size of the glyph cache,
    # which contains path and metrics information for glyphs, before it will
    # start purging entries. The current value matches the limit used by
    # Android.
    'default_font_cache_limit%': '(1*1024*1024)',
  },
  'conditions': [
    ['gl_type != "none"', {
      'variables': {
        'skia_export_defines': [
          'SK_SUPPORT_GPU=1',
        ],
      },
      'dependencies': [
        '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
      ],
    }, {
      'variables': {
        'skia_export_defines': [
          'SK_SUPPORT_GPU=0',
        ],
      },
    }],
    ['target_arch == "win"', {
      'variables': {
        'skia_export_defines': [
          # Required define by Skia to take certain code paths, such
          # as including windows.h in various places.
          'SK_BUILD_FOR_WIN32',
        ],
      },
    }],
  ],

  'defines': [
    '<@(skia_export_defines)',

    # skia uses static initializers to initialize the serialization logic
    # of its "pictures" library. This is currently not used in Cobalt; if
    # it ever gets used the processes that use it need to call
    # SkGraphics::Init().
    'SK_ALLOW_STATIC_GLOBAL_INITIALIZERS=0',

    # ETC1 texture encoding is not supported in Cobalt by default.
    'SK_IGNORE_ETC1_SUPPORT',

    'SK_DEFAULT_FONT_CACHE_LIMIT=<(default_font_cache_limit)',
  ],

  'direct_dependent_settings': {
    'defines': [
      '<@(skia_export_defines)',
    ],
  },

  # We would prefer this to be direct_dependent_settings,
  # however we currently have no means to enforce that direct dependents
  # re-export if they include Skia headers in their public headers.
  'all_dependent_settings': {
    'include_dirs': [
      'config',
    ],
  },

  'msvs_disabled_warnings': [4244, 4267, 4341, 4345, 4390, 4554, 4748, 4800],
}
