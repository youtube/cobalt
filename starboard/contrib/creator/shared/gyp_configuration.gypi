# Copyright 2016 Google Inc. All Rights Reserved.
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
    'target_arch': 'mips',
    'target_os': 'linux',

    'enable_webdriver': 0,
    'in_app_dial%': 0,
    'gl_type%': 'system_gles2',
    'rasterizer_type': 'direct-gles',

    'scratch_surface_cache_size_in_bytes' : 0,

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'mozjs-45',
    'cobalt_enable_jit': 1,
    'cobalt_media_source_2016': 1,

    'cobalt_font_package': 'expanded',
    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',
      '--sysroot=<(sysroot)',
      '-EL',

      # Suppress some warnings that will be hard to fix.
      '-Wno-unused-local-typedefs',
      '-Wno-unused-result',
      '-Wno-deprecated-declarations',
      '-Wno-missing-field-initializers',
      '-Wno-comment',
      '-Wno-narrowing',
      '-Wno-unknown-pragmas',
      '-Wno-type-limits',  # TODO: We should actually look into these.
      # Do not warn about sign compares.
      '-Wno-sign-compare',
    ],
    'linker_flags': [
      '--sysroot=<(sysroot)',
      '-EL',

      # We don't wrap these symbols, but this ensures that they aren't
      # linked in.
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
      '-O2',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-O2',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavresample',
      '-lavutil',
      '-lm',
      '-lpthread',
      '-lrt',
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
      ['clang==1', {
        'linker_flags': [
          '-fuse-ld=lld',
          '--target=mipsel-linux-gnu',
          '--gcc-toolchain=<(sysroot)/..',
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
    'conditions': [
      ['clang==1', {
        'cflags_c': [
          '-std=c11',
          '--target=mipsel-linux-gnu',
          '-march=mipsel',
          '-mcpu=mips32r2',
          '--gcc-toolchain=<(sysroot)/..',
        ],
        'cflags_cc': [
          '-std=gnu++11',
          '--target=mipsel-linux-gnu',
          '-march=mipsel',
          '-mcpu=mips32r2',
          '-Werror',
          '-fcolor-diagnostics',
          # Default visibility to hidden, to enable dead stripping.
          '-fvisibility=hidden',
          '-Wno-c++11-compat',
          # This complains about 'override', which we use heavily.
          '-Wno-c++11-extensions',
          # Warns on switches on enums that cover all enum values but
          # also contain a default: branch. Chrome is full of that.
          '-Wno-covered-switch-default',
          # protobuf uses hash_map.
          '-Wno-deprecated',
          '-fno-exceptions',
          # needed for backtrace()
          '-fasynchronous-unwind-tables',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          # Do not warn for implicit sign conversions.
          '-Wno-sign-conversion',
          '-fno-strict-aliasing',  # See http://crbug.com/32204
          '-Wno-unnamed-type-template-args',
          # Triggered by the COMPILE_ASSERT macro.
          '-Wno-unused-local-typedef',
          # Do not warn if a function or variable cannot be implicitly
          # instantiated.
          '-Wno-undefined-var-template',
          # Do not warn about an implicit exception spec mismatch.
          '-Wno-implicit-exception-spec-mismatch',
          '-Wno-tautological-constant-out-of-range-compare',
          '-Wno-undefined-inline',
        ],
      },{
        # gcc
        'cflags_c': [
          '-std=c11',
          '-EL',
        ],
        'cflags_cc': [
          '-std=gnu++11',
          '-Wno-literal-suffix',
          # needed for backtrace()
          '-fasynchronous-unwind-tables',
        ],
      }],
    ],
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
        ],
      },{
        'conditions': [
          ['clang==1', {
            'cflags': [
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
              '-Wno-undefined-var-template',
            ],
          },{ # gcc
            'cflags': [
              '-Wno-multichar',
            ],
          }],
        ],
      }],
    ],
  }, # end of target_defaults
}
