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
        '<(ft2_dir)/src/base/ftfntfmt.c',
        '<(ft2_dir)/src/base/ftfstype.c',
        '<(ft2_dir)/src/base/ftgasp.c',
        '<(ft2_dir)/src/base/ftglyph.c',
        '<(ft2_dir)/src/base/ftinit.c',
        '<(ft2_dir)/src/base/ftlcdfil.c',
        '<(ft2_dir)/src/base/ftmm.c',
        '<(ft2_dir)/src/base/ftstroke.c',
        '<(ft2_dir)/src/base/ftsystem.c',
        '<(ft2_dir)/src/base/fttype1.c',
        '<(ft2_dir)/src/cff/cff.c',
        '<(ft2_dir)/src/gzip/ftgzip.c',
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
        'FT_CONFIG_MODULES_H="ftmodule.h"',
        'FT_CONFIG_OPTIONS_H="ftoption.h"',
      ],
      'include_dirs': [
        '<(ft2_dir)/include_cobalt',
        '<(ft2_dir)/include',
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
      },
      'msvs_disabled_warnings': [4146],
    },
  ], # targets
}
