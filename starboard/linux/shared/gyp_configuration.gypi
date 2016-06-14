# Copyright 2014 Google Inc. All Rights Reserved.
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

# Platform specific configuration for Linux on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#
# TODO(***REMOVED***): Replace linux GYP configuration with this file.
{
  'variables': {
    'target_arch%': 'x64',
    'target_os': 'linux',
    #'starboard_path%': 'starboard/linux/x64x11',

    'enable_webdriver': '1',
    'in_app_dial%': 1,
    'gl_type%': 'system_gles3',

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'javascriptcore',

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',
    ],
    'linker_flags': [
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
      '-O2',
      '-gline-tables-only',
    ],
    'compiler_flags_gold': [
      '-fno-rtti',
      '-O2',
      '-gline-tables-only',
    ],
    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavresample',
      '-lavutil',
      '-lpthread',
      '-lpulse',
      '-lrt',
    ],
    'conditions': [
      ['clang==1', {
        'common_clang_flags': [
          '-Werror',
          '-fcolor-diagnostics',
          # Default visibility to hidden, to enable dead stripping.
          '-fvisibility=hidden',
          # Warn for implicit type conversions that may change a value.
          '-Wconversion',
          '-Wno-c++11-compat',
          # This (rightfully) complains about 'override', which we use
          # heavily.
          '-Wno-c++11-extensions',
          # Warns on switches on enums that cover all enum values but
          # also contain a default: branch. Chrome is full of that.
          '-Wno-covered-switch-default',
          # protobuf uses hash_map.
          '-Wno-deprecated',
          '-fno-exceptions',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          # Do not warn for implicit sign conversions.
          '-Wno-sign-conversion',
          '-fno-strict-aliasing',  # See http://crbug.com/32204
          # TODO(pkasting): In C++11 this is legal, so this should be
          # removed when we change to that.  (This is also why we don't
          # bother fixing all these cases today.)
          '-Wno-unnamed-type-template-args',
          # Triggered by the COMPILE_ASSERT macro.
          '-Wno-unused-local-typedef',
        ],
      }],
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
      ['use_asan==0', {
        'linker_flags': [
          # We don't wrap these symbols, but this ensures that they aren't
          # linked in. We do have to allow them to linked in when using ASAN, as
          # it needs to use its own version of these allocators in the Starboard
          # implementation.
          '-Wl,--wrap=malloc',
          '-Wl,--wrap=calloc',
          '-Wl,--wrap=realloc',
          '-Wl,--wrap=memalign',
          '-Wl,--wrap=reallocalign',
          '-Wl,--wrap=free',
          '-Wl,--wrap=strdup',
          '-Wl,--wrap=malloc_usable_size',
          '-Wl,--wrap=malloc_stats_fast',
          '-Wl,--wrap=__cxa_demangle',
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
      # Limit to C99. This allows Linux to be a canary build for any
      # C11 features that are not supported on some platforms' compilers.
      '-std=c99',
    ],
    'cflags_cc': [
      # Limit to gnu++98. This allows Linux to be a canary build for any
      # C++11 features that are not supported on some platforms' compilers.
      # We do allow ourselves GNU extensions, which are assumed to exist
      # by Chromium code.
      '-std=gnu++98',
    ],
    'target_conditions': [
      ['cobalt_code==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          '<@(common_clang_flags)',
        ],
      },{
        'cflags': [
          '<@(common_clang_flags)',
          # 'this' pointer cannot be NULL...pointer may be assumed
          # to always convert to true.
          '-Wno-undefined-bool-conversion',
          # Skia doesn't use overrides.
          '-Wno-inconsistent-missing-override',
          # Do not warn about unused function params.
          '-Wno-unused-parameter',
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
          # shifting a negative signed value is undefined
          '-Wno-shift-negative-value',
          # Width of bit-field exceeds width of its type- value will be truncated
          '-Wno-bitfield-width',
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
}
