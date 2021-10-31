# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'optimize_target_for_speed': 1,
  },
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
      'target_name': 'libwebp_dsp_dec',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_common_sources)',
      ],
      'dependencies': [
        'libwebp_dsp_dec_msa',
        'libwebp_dsp_dec_neon',
        'libwebp_dsp_dec_sse2',
        'libwebp_dsp_dec_sse41',
        'libwebp_dsp_dec_mips32',
        'libwebp_dsp_dec_mips_dsp_r2',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_msa',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_msa_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_neon',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_neon_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_sse2',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_sse2_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_sse41',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_sse41_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_mips32',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_mips32_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_dec_mips_dsp_r2',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_dec_mips_dsp_r2_sources)',
      ],
    },
    {
      'target_name': 'libwebp_utils_dec',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_utils_dec_sources)',
      ],
    },

    {
      'target_name': 'libwebp_enc',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_enc_sources)',
      ],
    },
    {
      'target_name': 'libwebp_dsp_enc',
      'type' : 'static_library',
      'include_dirs': ['.'],
      'sources' : [
        '<@(libwebp_dsp_enc_sources)',
      ],
      'dependencies': [
      ],
    },
    {
      'target_name': 'libwebp_utils_enc',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(libwebp_utils_enc_sources)',
      ],
    },

    {
      'target_name': 'libwebp',
      'type': 'none',
      'dependencies' : [
        'libwebp_dec',
        'libwebp_demux',
        'libwebp_dsp_dec',
        'libwebp_utils_dec',
        'libwebp_enc',
        'libwebp_dsp_enc',
        'libwebp_utils_enc',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
    },
  ],
}
