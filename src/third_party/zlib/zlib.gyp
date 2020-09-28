# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'optimize_target_for_speed': 1,
    'use_system_minizip%': 0,
    'use_arm_neon_optimizations%': 0,
    'use_x86_x64_optimizations%': 0,

    'conditions': [
      [ 'OS == "none"', {
        # Because we have a patched zlib, we cannot use the system libz.
        # TODO(pvalchev): OpenBSD is purposefully left out, as the system
        # zlib brings up an incompatibility that breaks rendering.
        'use_system_zlib%': 1,
      }, {
        'use_system_zlib%': 0,
      }],
      ['target_arch=="x86" or target_arch=="x64"', {
        'use_x86_x64_optimizations%': 1,
      }],
      ['target_arch=="arm" or target_arch=="arm64"', {
        'use_arm_neon_optimizations%': 1,
      }],
    ],
  },
  'targets': [
    # zlib_force_neon_fpu forces dependent targets to compile with the Advanced
    # SIMD extension.
    {
      'target_name': 'zlib_force_neon_fpu',
      'type': 'none',
      'conditions': [
        ['target_arch=="arm" and \
          use_arm_neon_optimizations==1 and \
          floating_point_fpu!="neon" and \
          floating_point_fpu!="neon-fp16" and \
          floating_point_fpu!="neon-vfpv4" and \
          floating_point_fpu!="crypto-neon-fp-armv8"', {
          'direct_dependent_settings': {
            'cflags!': [
              '-mfpu=<(floating_point_fpu)',
            ],
            'cflags': [
              '-mfpu=neon',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'zlib_adler32_simd',
      'type': 'static_library',
      'defines': [
        'ZLIB_IMPLEMENTATION',
      ],
      'conditions': [
        ['use_x86_x64_optimizations==1', {
          'defines': [
            'ADLER32_SIMD_SSSE3',
          ],
          'sources': [
            'adler32_simd.c',
            'adler32_simd.h',
          ],
          'direct_dependent_settings': {
            'defines': [
              'ADLER32_SIMD_SSSE3',
            ],
          },
          'conditions': [
            ['OS!="win" or clang==1', {
              'cflags': [
                '-mssse3',
              ],
            }],
          ],
        }],
        ['use_arm_neon_optimizations==1', {
          'defines': [
            'ADLER32_SIMD_NEON',
          ],
          'dependencies': [
            ':zlib_force_neon_fpu',
          ],
          'sources': [
            'adler32_simd.c',
            'adler32_simd.h',
          ],
          'direct_dependent_settings': {
            'defines': [
              'ADLER32_SIMD_NEON',
            ],
          },
          'conditions': [
            ['cobalt_config!="debug"', {
              'variables': {
                'optimize_target_for_speed': 1
              },
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'zlib_arm_crc32',
      'type': 'static_library',
      'conditions': [
        ['use_arm_neon_optimizations==1', {
          'include_dirs': [
            '.',
          ],
          'dependencies': [
            ':zlib_force_neon_fpu',
          ],
          'sources': [
            'contrib/optimizations/slide_hash_neon.c',
            'contrib/optimizations/slide_hash_neon.h',
          ],
          'conditions': [
            # CRC (cyclic redundancy check) instructions are only available for
            # ARM versions 8 and later.
            ['OS!="ios" and arm_version>=8', {
              'defines': [
                'CRC32_ARMV8_CRC32',
                'ZLIB_IMPLEMENTATION',
              ],
              'sources': [
                'crc32_simd.c',
                'crc32_simd.h',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'CRC32_ARMV8_CRC32',
                ],
              },
              'conditions': [
                ['OS=="android"', {
                  'defines': [
                    'ARMV8_OS_ANDROID',
                  ],
                  'dependencies': [
                    '<(android_ndk_root)/android_tools_ndk.gyp:cpu_features',
                  ],
                }],
                ['OS=="linux"', {
                  'defines': [
                    'ARMV8_OS_LINUX',
                  ],
                }],
                ['OS=="fuchsia"', {
                  'defines': [
                    'ARMV8_OS_FUCHSIA',
                  ],
                }],
                ['OS=="win"', {
                  'defines': [
                    'ARMV8_OS_WINDOWS',
                  ],
                }],
                ['OS!="win" and clang!=1', {
                  'cflags_c': [
                    '-march=armv8-a+crc',
                  ],
                }],
                ['cobalt_config!="debug"', {
                  'variables': {
                    'optimize_target_for_speed': 1,
                  },
                }],
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'zlib_inflate_chunk_simd',
      'type': 'static_library',
      'defines': [
        'ZLIB_IMPLEMENTATION',
      ],
      'conditions': [
        ['use_x86_x64_optimizations==1 or use_arm_neon_optimizations==1', {
          'include_dirs': [
            '.',
          ],
          'sources': [
            'contrib/optimizations/chunkcopy.h',
            'contrib/optimizations/inffast_chunk.c',
            'contrib/optimizations/inffast_chunk.h',
            'contrib/optimizations/inflate.c',
          ],
        }],
        ['use_x86_x64_optimizations==1', {
          'defines': [
            'INFLATE_CHUNK_SIMD_SSE2',
          ],
          'conditions': [
            ['target_arch=="x64"', {
              'defines': [
                'INFLATE_CHUNK_READ_64LE',
              ],
            }],
            ['target_arch=="x86"', {
              'cflags': [
                '-msse2',
              ],
            }],
          ],
        }],
        ['use_arm_neon_optimizations==1', {
          'defines': [
            'INFLATE_CHUNK_SIMD_NEON',
          ],
          'dependencies': [
            ':zlib_force_neon_fpu',
          ],
          'conditions': [
            ['target_arch=="arm64"', {
              'defines': [
                'INFLATE_CHUNK_READ_64LE',
              ],
            }],
            ['cobalt_config!="debug"', {
              'variables': {
                'optimize_target_for_speed': 1,
              },
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'zlib_crc32_simd',
      'type': 'static_library',
      'defines': [
        'ZLIB_IMPLEMENTATION',
      ],
      'conditions': [
        ['use_x86_x64_optimizations==1', {
          'defines': [
            'CRC32_SIMD_SSE42_PCLMUL',
          ],
          'sources': [
            'crc32_simd.c',
            'crc32_simd.h',
          ],
          'direct_dependent_settings': {
            'defines': [
              'CRC32_SIMD_SSE42_PCLMUL',
            ],
          },
          'conditions': [
            ['OS!="win" or clang==1', {
              'cflags': [
                '-msse4.2',
                '-mpclmul',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'zlib_x86_simd',
      'type': 'static_library',
      'defines': [
        'ZLIB_IMPLEMENTATION',
      ],
      'conditions': [
        ['use_x86_x64_optimizations==1', {
          'sources': [
            'crc_folding.c',
            'fill_window_sse.c',
          ],
          'conditions': [
            ['OS!="win" or clang==1', {
              'cflags': [
                '-msse4.2',
                '-mpclmul',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'zlib',
      'type': 'static_library',
      'conditions': [
        ['use_system_zlib==0', {
          'sources': [
            'adler32.c',
            'chromeconf.h',
            'compress.c',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.h',
            'inflate.c',
            'inftrees.c',
            'inftrees.h',
            'trees.c',
            'trees.h',
            'uncompr.c',
            'x86.h',
            'zconf.h',
            'zlib.h',
            'zutil.c',
            'zutil.h',
          ],
          'include_dirs': [
            '.',
          ],
          'all_dependent_settings': {
            'defines': [
               # To prevent Zlib ever from redefining const keyword in zconf.h
              'STDC',
            ]
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'defines': [
            'ZLIB_IMPLEMENTATION',
          ],
          'dependencies': [
            ':zlib_x86_simd',
          ],
          'conditions': [
            ['OS!="win"', {
              'product_name': 'chrome_zlib',
            }],
            ['OS=="android"', {
              'toolsets': ['target', 'host'],
            }],
            ['OS=="starboard"', {
              'sources!': [
                'gzclose.c',
                'gzguts.h',
                'gzlib.c',
                'gzread.c',
                'gzwrite.c',
              ],
            }],
            ['use_x86_x64_optimizations==1', {
              'defines': [
                'USE_X86_X64_OPTIMIZATIONS=1',
              ],
              'dependencies': [
                ':zlib_adler32_simd',
                ':zlib_crc32_simd',
                ':zlib_inflate_chunk_simd',
              ],
              'sources': [
                'x86.c',
              ],
            }, {
              'sources': [
                'simd_stub.c',
              ],
            }],
            ['use_arm_neon_optimizations==1', {
              'defines': [
                'USE_ARM_NEON_OPTIMIZATIONS=1',
              ],
              'dependencies': [
                ':zlib_adler32_simd',
                ':zlib_arm_crc32',
                ':zlib_inflate_chunk_simd',
              ],
              'sources': [
                # slide_hash_neon.h has been moved to zlib_arm_crc32 so that it
                # can be compiled with ARM NEON support.
                'arm_features.c',
                'arm_features.h',
              ],
            }, {
              'sources': [
                'arm_stub.c',
              ],
            }],
          ],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ZLIB',
            ],
          },
          'defines': [
            'USE_SYSTEM_ZLIB',
          ],
          'link_settings': {
            'libraries': [
              '-lz',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'minizip',
      'type': 'static_library',
      'conditions': [
        ['use_system_minizip==0', {
          'sources': [
            'contrib/minizip/ioapi.c',
            'contrib/minizip/ioapi.h',
            'contrib/minizip/iostarboard.c',
            'contrib/minizip/iostarboard.h',
            'contrib/minizip/iowin32.c',
            'contrib/minizip/iowin32.h',
            'contrib/minizip/unzip.c',
            'contrib/minizip/unzip.h',
            'contrib/minizip/zip.c',
            'contrib/minizip/zip.h',
          ],
          'include_dirs': [
            '.',
            '../..',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'dependencies': [
            ':zlib',
          ],
          'conditions': [
            ['OS!="win"', {
              'sources!': [
                'contrib/minizip/iowin32.c',
                'contrib/minizip/iowin32.h',
              ],
            }],
            ['OS=="android"', {
              'toolsets': ['target', 'host'],
            }],
          ],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_MINIZIP',
            ],
          },
          'defines': [
            'USE_SYSTEM_MINIZIP',
          ],
          'link_settings': {
            'libraries': [
              '-lminizip',
            ],
          },
        }],
        ['OS in ["mac", "ios", "android"] or os_bsd==1', {
          # Mac, Android and the BSDs don't have fopen64, ftello64, or
          # fseeko64. We use fopen, ftell, and fseek instead on these
          # systems.
          'defines': [
            'USE_FILE32API',
          ],
        }],
        ['clang==1', {
          'xcode_settings': {
            'WARNING_CFLAGS': [
              # zlib uses `if ((a == b))` for some reason.
              '-Wno-parentheses-equality',
            ],
          },
          'cflags': [
            '-Wno-parentheses-equality',
          ],
        }],
      ],
    },
    {
      'target_name': 'zip',
      'type': 'static_library',
      'sources': [
        'google/zip.cc',
        'google/zip.h',
        'google/zip_internal.cc',
        'google/zip_internal.h',
        'google/zip_reader.cc',
        'google/zip_reader.h',
        'google/zip_writer.cc',
        'google/zip_writer.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/zlib/zlib.gyp:minizip',
      ],
    },
    {
      'target_name': 'zip_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'google/compression_utils_unittest.cc',
        'google/zip_reader_unittest.cc',
        'google/zip_unittest.cc',

        # Required by the tests but not used elsewhere.
        'google/compression_utils.cc',
        'google/compression_utils.h',
        'google/compression_utils_portable.cc',
        'google/compression_utils_portable.h',
      ],
      'dependencies': [
        'minizip',
        'zip',
        'zlib',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/starboard/common/common.gyp:common',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'includes': ['<(DEPTH)/base/test/test.gypi'],
      'conditions': [
        ['cobalt_copy_test_data == 1', {
          'variables': {
            'content_test_input_files': [
              '<(DEPTH)/third_party/zlib/google/test/data',
            ],
            # Canonically this should be "third_party/zlib/google/test" to
            # match the source path, but we put it higher to reduce depth.
            'content_test_output_subdir': 'zlib',
          },
          'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
        }],
      ],
    },
    {
      'target_name': 'zip_unittests_deploy',
      'type': 'none',
      'dependencies': [
        'zip_unittests',
      ],
      'variables': {
        'executable_name': 'zip_unittests',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
