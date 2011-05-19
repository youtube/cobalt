# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'harfbuzz',
      'type': 'static_library',
      'sources': [
        'src/hb-blob-private.h',
        'src/hb-blob.c',
        'src/hb-blob.h',
        'src/hb-buffer-private.hh',
        'src/hb-buffer.cc',
        'src/hb-buffer.h',
        'src/hb-common.c',
        'src/hb-common.h',
        'src/hb-font-private.h',
        'src/hb-font.cc',
        'src/hb-font.h',
        'src/hb-ft.c',
        'src/hb-icu.c',
        'src/hb-language.c',
        'src/hb-language.h',
        'src/hb-object-private.h',
        'src/hb-open-file-private.hh',
        'src/hb-open-type-private.hh',
        'src/hb-ot-head-private.hh',
        'src/hb-ot-layout-common-private.hh',
        'src/hb-ot-layout-gdef-private.hh',
        'src/hb-ot-layout-gpos-private.hh',
        'src/hb-ot-layout-gsub-private.hh',
        'src/hb-ot-layout-gsubgpos-private.hh',
        'src/hb-ot-layout-private.hh',
        'src/hb-ot-layout.cc',
        'src/hb-ot-map-private.hh',
        'src/hb-ot-map.cc',
        'src/hb-ot-shape-complex-arabic-table.h',
        'src/hb-ot-shape-complex-arabic.cc',
        'src/hb-ot-shape-complex-private.hh',
        'src/hb-ot-shape-private.hh',
        'src/hb-ot-shape.cc',
        'src/hb-ot-tag.c',
        'src/hb-private.h',
        'src/hb-shape.cc',
        'src/hb-shape.h',
        'src/hb-unicode-private.h',
        'src/hb-unicode.c',
        'src/hb-unicode.h',
        'src/hb.h',
      ],
      'include_dirs': [
        'src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
      'dependencies': [
        '../../build/linux/system.gyp:freetype2',
        '../../third_party/icu/icu.gyp:icuuc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
