# Copyright 2020 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'conditions': [
    ['sb_api_version >= 12', {
      'variables': {
        'DAV1D_ARCH_AARCH64':   'SB_IS(ARCH_ARM64)',
        'DAV1D_ARCH_ARM':       'SB_IS(ARCH_ARM)',
        'DAV1D_ARCH_PPC64LE':   'SB_IS(ARCH_PPC)',
        'DAV1D_ARCH_X86':       '(SB_IS(ARCH_X86) || SB_IS(ARCH_X64))',
        'DAV1D_ARCH_X86_32':    'SB_IS(ARCH_X86)',
        'DAV1D_ARCH_X86_64':    'SB_IS(ARCH_X64)',
        'DAV1D_ENDIANNESS_BIG': 'SB_IS(BIG_ENDIAN)',
      },
    }, {
      'variables': {
        'DAV1D_ARCH_AARCH64':   '(SB_IS_ARCH_ARM & SB_IS_64_BIT)',
        'DAV1D_ARCH_ARM':       '(SB_IS_ARCH_ARM & SB_IS_32_BIT)',
        'DAV1D_ARCH_PPC64LE':   'SB_IS_ARCH_PPC',
        'DAV1D_ARCH_X86':       'SB_IS_ARCH_X86',
        'DAV1D_ARCH_X86_32':    '(SB_IS_ARCH_X86 & SB_IS_32_BIT)',
        'DAV1D_ARCH_X86_64':    '(SB_IS_ARCH_X86 & SB_IS_64_BIT)',
        'DAV1D_ENDIANNESS_BIG': 'SB_IS_BIG_ENDIAN',
      },
    }],
  ],

  'variables': {
    'enable_asm': 0,

    'libdav1d_dir': '<(DEPTH)/third_party/libdav1d',

    'libdav1d_include_dirs': [
      '<(libdav1d_dir)/',
      '<(libdav1d_dir)/include',
      '<(libdav1d_dir)/include/dav1d',
    ],

    # BITDEPTH-specific sources
    #  Note: these files must be compiled with -DBITDEPTH=8 or 16
    'libdav1d_bitdepth_sources': [
      'src/cdef.h',
      'src/cdef_apply.h',
      'src/cdef_apply_tmpl.c',
      'src/cdef_tmpl.c',
      'src/fg_apply.h',
      'src/fg_apply_tmpl.c',
      'src/film_grain.h',
      'src/film_grain_tmpl.c',
      'src/ipred.h',
      'src/ipred_prepare.h',
      'src/ipred_prepare_tmpl.c',
      'src/ipred_tmpl.c',
      'src/itx.h',
      'src/itx_tmpl.c',
      'src/lf_apply.h',
      'src/lf_apply_tmpl.c',
      'src/loopfilter.h',
      'src/loopfilter_tmpl.c',
      'src/looprestoration.h',
      'src/looprestoration_tmpl.c',
      'src/lr_apply.h',
      'src/lr_apply_tmpl.c',
      'src/mc.h',
      'src/mc_tmpl.c',
      'src/recon.h',
      'src/recon_tmpl.c',
    ],

    # ARCH-specific sources
    'libdav1d_arch_sources': [],
    'libdav1d_arch_bitdepth_sources': [],

    # ASM-specific sources
    'libdav1d_base_asm_sources': [],
    'libdav1d_bitdepth8_asm_sources': [],
    'libdav1d_bitdepth16_asm_sources': [],

    'conditions': [
      ['enable_asm == 1 and (target_arch == "x86" or target_arch == "x64")', {
        'libdav1d_arch_sources': [
          'src/x86/cpu.c',
          'src/x86/cpu.h',
          'src/x86/msac.h',
        ],
        'libdav1d_arch_bitdepth_sources': [
          'src/x86/cdef_init_tmpl.c',
          'src/x86/film_grain_init_tmpl.c',
          'src/x86/ipred_init_tmpl.c',
          'src/x86/itx_init_tmpl.c',
          'src/x86/loopfilter_init_tmpl.c',
          'src/x86/looprestoration_init_tmpl.c',
          'src/x86/mc_init_tmpl.c',
        ],
        'libdav1d_base_asm_sources': [
          'src/x86/cpuid.asm',
          'src/x86/msac.asm',
        ],
        'libdav1d_bitdepth8_asm_sources': [
          'src/x86/cdef.asm',
          'src/x86/cdef_sse.asm',
          'src/x86/film_grain.asm',
          'src/x86/ipred.asm',
          'src/x86/ipred_ssse3.asm',
          'src/x86/itx.asm',
          'src/x86/itx_ssse3.asm',
          'src/x86/loopfilter.asm',
          'src/x86/loopfilter_ssse3.asm',
          'src/x86/looprestoration.asm',
          'src/x86/looprestoration_ssse3.asm',
          'src/x86/mc.asm',
          'src/x86/mc_ssse3.asm',
        ],
      }],
    ],
  },

  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/third_party/libdav1d/',
      '<(DEPTH)/third_party/libdav1d/include',
      '<(DEPTH)/third_party/libdav1d/include/dav1d',
      '<(DEPTH)/third_party/libdav1d/src',
    ],
    'defines': [
      'ARCH_AARCH64=<(DAV1D_ARCH_AARCH64)',
      'ARCH_ARM=<(DAV1D_ARCH_ARM)',
      'ARCH_PPC64LE=<(DAV1D_ARCH_PPC64LE)',
      'ARCH_X86=<(DAV1D_ARCH_X86)',
      'ARCH_X86_32=<(DAV1D_ARCH_X86_32)',
      'ARCH_X86_64=<(DAV1D_ARCH_X86_64)',
      'CONFIG_16BPC=1',
      'CONFIG_8BPC=1',
      'CONFIG_LOG=1',
      'ENDIANNESS_BIG=<(DAV1D_ENDIANNESS_BIG)',
      'HAVE_ASM=<(enable_asm)',
    ],

    'conditions': [
      ['target_os == "linux"', {
        'defines':[
          'HAVE_CLOCK_GETTIME=1',
          'HAVE_POSIX_MEMALIGN=1',
          'HAVE_UNISTD_H=1',
          'STACK_ALIGNMENT=32',
        ]
      }],

      ['target_os == "win"', {
        'include_dirs': [
          # for stdatomic.h
          '<(DEPTH)/third_party/libdav1d/include/compat/msvc',
        ],
        'defines':[
          'HAVE_ALIGNED_MALLOC=1',
          'HAVE_IO_H=1',
          'UNICODE=1',
          '_CRT_DECLARE_NONSTDC_NAMES=1',
          '_UNICODE=1',
          '_WIN32_WINNT=0x0601',
          '__USE_MINGW_ANSI_STDIO=1',
          'fseeko=_fseeki64',
          'ftello=_ftelli64',
        ],
        'cflags_cc': [
          '-wd4028',
          '-wd4996',
        ],
        'conditions': [
          ['target_arch == "x64"', {
            'defines': [
              'STACK_ALIGNMENT=16',
            ],
          }, {
            'defines': [
              'STACK_ALIGNMENT=4',
            ],
          }],
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'libdav1d_entrypoint',
      'type': 'static_library',
      'sources': [
        'src/lib.c',
        'src/thread_task.c',
        'src/thread_task.h',
      ],
    },
    {
      'target_name': 'libdav1d_bitdepth16',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=16',
      ],
      'sources': [
        '<@(libdav1d_arch_bitdepth_sources)',
        '<@(libdav1d_bitdepth_sources)',
      ],
    },
    {
      'target_name': 'libdav1d_bitdepth16_asm',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=16',
      ],
      'includes': [
        'libdav1d_asm.gypi'
      ],
      'sources': [
        '<@(libdav1d_bitdepth16_asm_sources)',
      ],
    },
    {
      'target_name': 'libdav1d_bitdepth8',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=8',
      ],
      'sources': [
        '<@(libdav1d_arch_bitdepth_sources)',
        '<@(libdav1d_bitdepth_sources)',
      ],
    },
    {
      'target_name': 'libdav1d_bitdepth8_asm',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=8',
      ],
      'includes': [
        'libdav1d_asm.gypi'
      ],
      'sources': [
        '<@(libdav1d_bitdepth8_asm_sources)',
      ],
    },
    {
      'target_name': 'libdav1d_base',
      'type': 'static_library',
      'sources': [
        'src/cpu.c',
        'src/cpu.h',
        'src/msac.c',
        'src/msac.h',
      ],
    },
    {
      'target_name': 'libdav1d_base_asm',
      'type': 'static_library',
      'includes': [
        'libdav1d_asm.gypi'
      ],
      'sources': [
        '<@(libdav1d_arch_sources)',
        '<@(libdav1d_base_asm_sources)'
      ],
    },
    {
      'target_name': 'libdav1d',
      'type': 'static_library',
      'sources': [
        'src/cdf.c',
        'src/cdf.h',
        'src/ctx.h',
        'src/data.c',
        'src/data.h',
        'src/decode.c',
        'src/decode.h',
        'src/dequant_tables.c',
        'src/dequant_tables.h',
        'src/env.h',
        'src/getbits.c',
        'src/getbits.h',
        'src/internal.h',
        'src/intra_edge.c',
        'src/intra_edge.h',
        'src/levels.h',
        'src/lf_mask.c',
        'src/lf_mask.h',
        'src/log.c',
        'src/log.h',
        'src/obu.c',
        'src/obu.h',
        'src/picture.c',
        'src/picture.h',
        'src/qm.c',
        'src/qm.h',
        'src/ref.c',
        'src/ref.h',
        'src/ref_mvs.c',
        'src/ref_mvs.h',
        'src/scan.c',
        'src/scan.h',
        'src/tables.c',
        'src/tables.h',
        'src/thread.h',
        'src/thread_data.h',
        'src/warpmv.c',
        'src/warpmv.h',
        'src/wedge.c',
        'src/wedge.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/common/common.gyp:common',
        'libdav1d_base',
        'libdav1d_base_asm',
        'libdav1d_bitdepth16',
        'libdav1d_bitdepth16_asm',
        'libdav1d_bitdepth8',
        'libdav1d_bitdepth8_asm',
        'libdav1d_entrypoint',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(libdav1d_include_dirs)',
        ],
      },
    },
  ],
}
