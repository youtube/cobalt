# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'variables': {
    'ft2_dir': '<(DEPTH)/third_party/freetype2',
 },
 'targets': [
    {
      'target_name': 'freetype2',
      'type': 'static_library',
      'toolsets': ['target'],
      'sources': [
        '<(ft2_dir)/src/autofit/autofit.c',
        '<(ft2_dir)/src/base/ftbase.c',
        '<(ft2_dir)/src/base/ftbbox.c',
        '<(ft2_dir)/src/base/ftbitmap.c',
        '<(ft2_dir)/src/base/ftdebug.c',
        '<(ft2_dir)/src/base/ftfstype.c',
        '<(ft2_dir)/src/base/ftgasp.c',
        '<(ft2_dir)/src/base/ftglyph.c',
        '<(ft2_dir)/src/base/ftinit.c',
        '<(ft2_dir)/src/base/ftmm.c',
        '<(ft2_dir)/src/base/ftstroke.c',
        '<(ft2_dir)/src/base/ftsystem.c',
        '<(ft2_dir)/src/base/fttype1.c',
        '<(ft2_dir)/src/cff/cff.c',
        '<(ft2_dir)/src/gzip/ftgzip.c',
        '<(ft2_dir)/src/psaux/psaux.c',
        '<(ft2_dir)/src/pshinter/pshinter.c',
        '<(ft2_dir)/src/psnames/psnames.c',
        '<(ft2_dir)/src/raster/raster.c',
        '<(ft2_dir)/src/sfnt/sfnt.c',
        '<(ft2_dir)/src/smooth/smooth.c',
        '<(ft2_dir)/src/truetype/truetype.c',
      ],
      'defines': [
        'FT_CONFIG_OPTION_SYSTEM_ZLIB',
        'FT2_BUILD_LIBRARY',
        'FT_CONFIG_CONFIG_H="ftconfig.h"',
        'FT_CONFIG_MODULES_H="ftmodule.h"',
        'FT_CONFIG_OPTIONS_H="ftoption.h"',
      ],
      'include_dirs': [
        '<(ft2_dir)/include_cobalt',
        '<(ft2_dir)/include',
        '<(DEPTH)/third_party/brotli/c/include',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(ft2_dir)/include_cobalt',
          '<(ft2_dir)/include',
        ],
        'defines': [
          'FT_CONFIG_OPTION_SYSTEM_ZLIB',
          'FT_CONFIG_CONFIG_H="ftconfig.h"',
          'FT_CONFIG_MODULES_H="ftmodule.h"',
          'FT_CONFIG_OPTIONS_H="ftoption.h"',
        ],
      },
      'msvs_disabled_warnings': [
        # Level 1 - Formal parameter 'number' is different from declaration.
        4028,
        # Level 1 - Incompatible types conversion.
        4133,
        # Level 2 - Unary minus operator applied to unsigned type; result is
        # still unsigned.
        4146,
        # Level 1 - Conversion from 'type1' to 'type2' of a greater size.
        # Typically when 32-bit value is assigned to a 64-bit pointer value.
        4312,
      ],
      'conditions': [
        ['clang == 1', {
          'cflags': [
            '-Wno-tautological-compare',
          ],
        }],
      ],
    },
  ], # targets
}
