# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': [
    'libwebp.gypi'
  ],
  'targets': [
    {
      'target_name': 'libwebp_dec',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_dec_sources)',
      ],
    },
    {
      'target_name': 'libwebp_demux',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_demux_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_common_sources)',
        '<@(libwebp_dsp_enc_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_avx2',
        'libwebp_dsp_msa',
        'libwebp_dsp_neon',
        'libwebp_dsp_sse2',
        'libwebp_dsp_sse41',
      ],
      'conditions': [
        ['OS == "android"', {
          'includes': [ '../../build/android/cpufeatures.gypi' ],
        }],
      ],
    },
    {
      'target_name': 'libwebp_dsp_avx2',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_avx2_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_decode_msa',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_decode_msa_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_decode_neon',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_decode_neon_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_decode_sse41',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_decode_sse41_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_decode_sse2',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_decode_sse2_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_msa',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_msa_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_decode_msa',
      ]
    },
    {
      'target_name': 'libwebp_dsp_neon',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_neon_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_decode_neon',
      ]
    },
    {
      'target_name': 'libwebp_dsp_sse2',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_sse2_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_decode_sse2',
      ],
    },
    {
      'target_name': 'libwebp_dsp_sse41',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_sse41_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_decode_sse41',
      ]
    },
    {
      'target_name': 'libwebp_enc',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_enc_sources)',
      ],
    },
    {
      'target_name': 'libwebp_utils',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_utils_common_sources)',
        '<@(libwebp_utils_enc_sources)',
      ],
    },
    {
      'target_name': 'libwebp',
      'type': 'none',
      'dependencies' : [
        'libwebp_dec',
        'libwebp_demux',
        'libwebp_dsp',
        'libwebp_enc',
        'libwebp_utils',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'conditions': [
        ['OS!="win"', {'product_name': 'webp'}],
      ],
    },
  ],
}
