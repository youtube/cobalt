# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
  'variables': {
    # Override that omits the "data" subdirectory.
    # TODO: Remove when omitted for all platforms in base_configuration.gypi.
    'sb_static_contents_output_data_dir': '<(PRODUCT_DIR)/content',

    'target_os': 'linux',

    'sysroot%': '/',
    'gl_type': 'system_gles2',

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # Force char to be signed.
      '-fsigned-char',

      # Disable strict aliasing.
      '-fno-strict-aliasing',

      # Allow Skia's SkVx.h to convert between vectors of different element
      # types or number of subparts.
      '-flax-vector-conversions',

      # To support large files
      '-D_FILE_OFFSET_BITS=64',

      # Suppress some warnings that will be hard to fix.
      '-Wno-unused-local-typedefs',
      '-Wno-unused-result',
      '-Wno-unused-function',
      '-Wno-deprecated-declarations',
      '-Wno-missing-field-initializers',
      '-Wno-extra',
      '-Wno-comment',  # Talk to my lawyer.
      '-Wno-narrowing',
      '-Wno-unknown-pragmas',
      '-Wno-type-limits',  # TODO: We should actually look into these.
      # It's OK not to use some input parameters. Note that the order
      # matters: Wall implies Wunused-parameter and Wno-unused-parameter
      # has no effect if specified before Wall.
      '-Wno-unused-parameter',

      # Specify the sysroot with all your include dependencies.
      '--sysroot=<(sysroot)',

      # This is a quirk of Raspbian, these are required to include any
      # GL-related headers.
      '-I<(sysroot)/opt/vc/include',
      '-I<(sysroot)/opt/vc/include/interface/vcos/pthreads',
      '-I<(sysroot)/opt/vc/include/interface/vmcs_host/linux',
      '-I<(sysroot)/usr/include/arm-linux-gnueabihf',
    ],
    'linker_flags': [
      '--sysroot=<(sysroot)',
      # This is a quirk of Raspbian, these are required to link any GL-related
      # libraries.
      '-L<(sysroot)/opt/vc/lib',
      '-Wl,-rpath=<(sysroot)/opt/vc/lib',
      '-L<(sysroot)/usr/lib/arm-linux-gnueabihf',
      '-Wl,-rpath=<(sysroot)/usr/lib/arm-linux-gnueabihf',
      '-L<(sysroot)/lib/arm-linux-gnueabihf',
      '-Wl,-rpath=<(sysroot)/lib/arm-linux-gnueabihf',
      # Cleanup unused sections
      '-Wl,-gc-sections',
      '-Wl,--unresolved-symbols=ignore-in-shared-libs',
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_cc_debug': [
      '-frtti',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_devel': [
      '-frtti',
    ],
    'compiler_flags_qa': [
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_qa_size': [
      '-Os',
      # Compile symbols in separate sections
      '-ffunction-sections',
      '-fdata-sections',
    ],
    'compiler_flags_qa_speed': [
      '-O2',
      # Compile symbols in separate sections
      '-ffunction-sections',
      '-fdata-sections',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_gold_size': [
      '-Os',
      # Compile symbols in separate sections
      '-ffunction-sections',
      '-fdata-sections',
    ],
    'compiler_flags_gold_speed': [
      '-O2',
      # Compile symbols in separate sections
      '-ffunction-sections',
      '-fdata-sections',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavutil',
      '-l:libpthread.so.0',
      '-lpthread',
      '-lrt',
      '-lopenmaxil',
      '-lbcm_host',
      '-lvcos',
      '-lvchiq_arm',
      '-lbrcmGLESv2',
      '-lbrcmEGL',
      # Static libs must be last, to avoid __dlopen linker errors
      '-lEGL_static',
      '-lGLESv2_static',
    ],
    'conditions': [
      ['cobalt_fastbuild==0', {
        'compiler_flags_debug': [
          '-g',
        ],
        'compiler_flags_devel': [
          '-g',
        ],
        'compiler_flags_qa': [
        ],
        'compiler_flags_gold': [
        ],
      }],
    ],
  },

  'target_defaults': {
    'defines': [
      # Cobalt on Linux flag
      'COBALT_LINUX',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
    ],
    'cflags_c': [
      '-std=c11',
    ],
    'cflags_cc': [
      '-std=gnu++14',
      '-Wno-literal-suffix',
    ],
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          # Raspi toolchain is based off an old version of gcc, which
          # falsely flags some code.  That same code does not warn with gcc 6.3.
          # This decision should be revisited after raspi toolchain is upgraded.
          '-Wno-maybe-uninitialized',
          # Turn warnings into errors.
          '-Werror',
          '-Wno-expansion-to-defined',
          '-Wno-implicit-fallthrough',
        ],
      },{
        'cflags': [
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
        ],
      }],
    ],
  }, # end of target_defaults

  'includes': [
    '<(DEPTH)/starboard/sabi/sabi.gypi',
  ],
}
