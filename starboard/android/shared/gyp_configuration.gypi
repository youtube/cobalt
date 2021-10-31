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

# Platform specific configuration for Android on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#
{
  'variables': {
    'target_os': 'android',
    'final_executable_type': 'shared_library',
    'gtest_target_type': 'shared_library',
    'sb_widevine_platform': 'android',
    'sb_enable_benchmark': 1,
    'cobalt_licenses_platform': 'android',

    'gl_type': 'system_gles2',
    'enable_remote_debugging': 0,

    'enable_vulkan%': 0,

    'linker_flags': [
      # The NDK default "ld" is actually the gold linker for all architectures
      # except arm64 (aarch64) where it's the bfd linker. Don't use either of
      # those, rather use lld everywhere. See release notes for NDK 19:
      # https://developer.android.com/ndk/downloads/revision_history
      '-fuse-ld=lld',
    ],

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags_debug': [
      '-frtti',
      '-O0',
    ],
    'compiler_flags_devel': [
      '-frtti',
      '-O2',
    ],
    'compiler_flags_qa': [
      '-fno-rtti',
      '-gline-tables-only',
    ],
    'compiler_flags_qa_size': [
      '-Os',
    ],
    'compiler_flags_qa_speed': [
      '-O2',
    ],
    'compiler_flags_gold': [
      '-fno-rtti',
      '-gline-tables-only',
    ],
    'compiler_flags_gold_size': [
      '-Os',
    ],
    'compiler_flags_gold_speed': [
      '-O2',
    ],
    'platform_libraries': [
      '-lEGL',
      '-lGLESv2',
      '-lOpenSLES',
      '-landroid',
      '-llog',
      '-lmediandk',
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
          '-gline-tables-only',
        ],
        'compiler_flags_gold': [
          '-gline-tables-only',
        ],
      }],
    ],
  },

  'target_defaults': {
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          # Don't get pedantic about warnings from base macros. These must be
          # disabled after the -Wall above, so this has to be done here rather
          # than in the platform's target toolchain.
          # TODO: Rebase base and use static_assert instead of COMPILE_ASSERT
          '-Wno-unused-local-typedef',  # COMPILE_ASSERT
          '-Wno-missing-field-initializers',  # LAZY_INSTANCE_INITIALIZER
          # It's OK not to use some input parameters. Note that the order
          # matters: Wall implies Wunused-parameter and Wno-unused-parameter
          # has no effect if specified before Wall.
          '-Wno-unused-parameter',
        ],
      }],
      ['_type=="executable"', {
        # Android Lollipop+ requires relocatable executables.
        'cflags': [
          '-fPIE',
        ],
        'ldflags': [
          '-pie',
        ],
      },{
        # Android requires relocatable shared libraries.
        'cflags': [
          '-fPIC',
        ],
      }],
      ['use_asan==1', {
        'cflags': [
          '-fsanitize=address',
          '-fno-omit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=address',
          # Force linking of the helpers in sanitizer_options.cc
          '-Wl,-u_sanitizer_options_link_helper',
        ],
        'defines': [
          'ADDRESS_SANITIZER',
        ],
      }],
      ['use_tsan==1', {
        'cflags': [
          '-fsanitize=thread',
          '-fno-omit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=thread',
        ],
        'defines': [
          'THREAD_SANITIZER',
        ],
      }],
    ],
  }, # end of target_defaults

  'includes': [
    '<(DEPTH)/starboard/sabi/sabi.gypi',
  ],
}
