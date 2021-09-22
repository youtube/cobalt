# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'optimize_target_for_speed': 1,
    'is_starboard': 1,
  },
  'targets': [
    {
      'target_name': 'libjpeg',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'defines': [
        'BMP_SUPPORTED',
        'PPM_SUPPORTED',
      ],
      'variables': {
        'no_simd_files': [
          'jsimd.h',
          'jsimd_none.c',
        ]
      },
      'sources': [
        'cdjpeg.h',
        'jaricom.c',
        'jcapimin.c',
        'jcapistd.c',
        'jcarith.c',
        'jccoefct.c',
        'jccolor.c',
        'jcdctmgr.c',
        'jchuff.c',
        'jchuff.h',
        'jcinit.c',
        'jcmainct.c',
        'jcmarker.c',
        'jcmaster.c',
        'jcomapi.c',
        'jconfig.h',
        'jconfigint.h',
        'jcparam.c',
        'jcphuff.c',
        'jcprepct.c',
        'jcsample.c',
        'jctrans.c',
        'jdapimin.c',
        'jdapistd.c',
        'jdarith.c',
        'jdatadst.c',
        'jdatadst-tj.c',
        'jdatasrc.c',
        'jdatasrc-tj.c',
        'jdcoefct.c',
        'jdcolor.c',
        'jdct.h',
        'jddctmgr.c',
        'jdhuff.c',
        'jdhuff.h',
        'jdinput.c',
        'jdmainct.c',
        'jdmarker.c',
        'jdmaster.c',
        'jdmerge.c',
        'jdphuff.c',
        'jdpostct.c',
        'jdsample.c',
        'jdtrans.c',
        'jerror.c',
        'jerror.h',
        'jfdctflt.c',
        'jfdctfst.c',
        'jfdctint.c',
        'jidctflt.c',
        'jidctfst.c',
        'jidctint.c',
        'jidctred.c',
        'jinclude.h',
        'jmemmgr.c',
        'jmemnobs.c',
        'jmemsys.h',
        'jmorecfg.h',
        'jpeg_nbits_table.c',
        'jpegint.h',
        'jpeglib.h',
        'jpeglibmangler.h',
        'jquant1.c',
        'jquant2.c',
        'jutils.c',
        'jversion.h',
        'rdbmp.c',
        'rdppm.c',
        'transupp.h',
        'transupp.c',
        'turbojpeg.h',
        'turbojpeg.c',
        'wrbmp.c',
        'wrppm.c',
        '<@(no_simd_files)',
      ],
      'conditions': [
        ['is_starboard', {
          # These dependecies are needed for file io
          # and are not currently used by Cobalt.
          'sources!': [
            'jdatadst.c',
            'jdatasrc.c',
            'rdbmp.c',
            'rdppm.c',
            'wrbmp.c',
            'wrppm.c',
          ],
        }],
        # arm processor specific optimizations.
        ['target_arch == "arm" and arm_neon == 1 or target_arch == "arm64" and arm_neon == 1', {
          'include_dirs': [
            'simd/arm',
          ],
          'sources!': [
            '<@(no_simd_files)'
          ],
          'sources': [
            'simd/arm/jccolor-neon.c',
            'simd/arm/jcgray-neon.c',
            'simd/arm/jcphuff-neon.c',
            'simd/arm/jcsample-neon.c',
            'simd/arm/jdcolor-neon.c',
            'simd/arm/jdmerge-neon.c',
            'simd/arm/jdsample-neon.c',
            'simd/arm/jfdctfst-neon.c',
            'simd/arm/jfdctint-neon.c',
            'simd/arm/jidctfst-neon.c',
            'simd/arm/jidctint-neon.c',
            'simd/arm/jidctred-neon.c',
            'simd/arm/jquanti-neon.c',
          ],
          'defines': [
            'WITH_SIMD',
            'NEON_INTRINSICS',
          ],
          'conditions': [
            ['target_arch == "arm"', {
              'sources': [
                'simd/arm/aarch32/jsimd.c',
                'simd/arm/aarch32/jchuff-neon.c',
              ],
            }],
            ['target_arch == "arm64"', {
              'sources': [
                'simd/arm/aarch64/jsimd.c',
                'simd/arm/aarch64/jchuff-neon.c',
              ],
            }],
            # For all Evergreen hardfp and softfp platforms.
            ['floating_point_fpu=="vfpv3"', {
              'cflags!': [
                '-mfpu=<(floating_point_fpu)',
              ],
              'cflags': [
                '-mfpu=neon',
              ],
            }],
          ],
        }],
        # x86_64 specific optimizations.
        ['<(yasm_exists) == 1 and target_arch == "x64"', {
          'sources!': [
            '<@(no_simd_files)'
          ],
          'sources': [
            'simd/x86_64/jsimdcpu.asm',
            'simd/x86_64/jfdctflt-sse.asm',
            'simd/x86_64/jccolor-sse2.asm',
            'simd/x86_64/jcgray-sse2.asm',
            'simd/x86_64/jchuff-sse2.asm',
            'simd/x86_64/jcphuff-sse2.asm',
            'simd/x86_64/jcsample-sse2.asm',
            'simd/x86_64/jdcolor-sse2.asm',
            'simd/x86_64/jdmerge-sse2.asm',
            'simd/x86_64/jdsample-sse2.asm',
            'simd/x86_64/jfdctfst-sse2.asm',
            'simd/x86_64/jfdctint-sse2.asm',
            'simd/x86_64/jidctflt-sse2.asm',
            'simd/x86_64/jidctfst-sse2.asm',
            'simd/x86_64/jidctint-sse2.asm',
            'simd/x86_64/jidctred-sse2.asm',
            'simd/x86_64/jquantf-sse2.asm',
            'simd/x86_64/jquanti-sse2.asm',
            'simd/x86_64/jccolor-avx2.asm',
            'simd/x86_64/jcgray-avx2.asm',
            'simd/x86_64/jcsample-avx2.asm',
            'simd/x86_64/jdcolor-avx2.asm',
            'simd/x86_64/jdmerge-avx2.asm',
            'simd/x86_64/jdsample-avx2.asm',
            'simd/x86_64/jfdctint-avx2.asm',
            'simd/x86_64/jidctint-avx2.asm',
            'simd/x86_64/jquanti-avx2.asm',
            'simd/x86_64/jsimd.c',
            'simd/jsimd.h',
          ],
          # Rules for assembling .asm files with yasm.
          'rules': [
            {
              'rule_name': 'assemble',
              'extension': 'asm',
                  'inputs': [ ],
                  'outputs': [
                    '<(PRODUCT_DIR)/obj/third_party/libjpeg-turbo/<(RULE_INPUT_ROOT).asm.o',
                  ],
                  'action': [
                    '<(path_to_yasm)',
                    '-DELF',
                    '-D__x86_64__',
                    '-DPIC',
                    '-Isimd/nasm/',
                    '-Isimd/x86_64/',
                    '-f',
                    'elf64',
                    '-o',
                    "<(PRODUCT_DIR)/$out",
                    '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 1,
                  'message': 'Building <(RULE_INPUT_ROOT).asm.o',
            },
          ],
          },
        ]
      ],
    },
  ],
}
