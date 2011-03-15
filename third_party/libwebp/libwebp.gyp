# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libwebp%': 0,
  },
  'conditions': [
    ['use_system_libwebp==0', {
      'targets': [
        {
          'target_name': 'libwebp_enc',
          'type': '<(library)',
          'sources': [
            'enc/analysis.c',
            'enc/bit_writer.c',
            'enc/config.c',
            'enc/cost.c',
            'enc/dsp.c',
            'enc/filter.c',
            'enc/frame.c',
            'enc/iterator.c',
            'enc/picture.c',
            'enc/quant.c',
            'enc/syntax.c',
            'enc/tree.c',
            'enc/webpenc.c'
          ],
        },
        {
          'target_name': 'libwebp_dec',
          'type': '<(library)',
          'sources': [
            'dec/bits.c',
            'dec/dsp.c',
            'dec/frame.c',
            'dec/quant.c',
            'dec/tree.c',
            'dec/vp8.c',
            'dec/webp.c',
            'dec/yuv.c',
          ],
        },
        {
          'target_name': 'libwebp',
          'type': '<(library)',
          'dependencies' : [
            'libwebp_enc',
            'libwebp_dec',
          ],
          'direct_dependent_settings': {
            'include_dirs': ['.'],
          },
          'conditions': [
            ['OS!="win"', {'product_name': 'webp'}],
          ],
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'libwebp',
          'type': 'settings',
          'direct_dependent_settings': {
            'defines': [
              'ENABLE_WEBP',
            ],
          },
          'link_settings': {
            'libraries': [
              '-lwebp',
            ],
          },
        }
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
