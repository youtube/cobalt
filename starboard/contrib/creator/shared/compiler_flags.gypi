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

# Platform specific compiler flags for Linux on Starboard. Included from
# gyp_configuration.gypi.
#
{
  'variables': {
    'compiler_flags_host': [
      '-O2',
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
    'conditions': [
      ['clang==1', {
        'linker_flags': [
          '-fuse-ld=lld',
          '--target=mipsel-linux-gnu',
          '--gcc-toolchain=<(sysroot)/..',
        ],
        'common_clang_flags': [
          '-Werror',
          '-fcolor-diagnostics',
          # Default visibility to hidden, to enable dead stripping.
          '-fvisibility=hidden',
          # Warn for implicit type conversions that may change a value.
          '-Wconversion',
          '-Wno-c++11-compat',
          # This complains about 'override', which we use heavily.
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
          '-Wno-unnamed-type-template-args',
          # Triggered by the COMPILE_ASSERT macro.
          '-Wno-unused-local-typedef',
          # Do not warn if a function or variable cannot be implicitly
          # instantiated.
          '-Wno-undefined-var-template',
          # Do not warn about an implicit exception spec mismatch.
          '-Wno-implicit-exception-spec-mismatch',
          # needed for backtrace()
          '-fasynchronous-unwind-tables',
          '-Wno-tautological-constant-out-of-range-compare',
          '-Wno-undefined-inline',
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
    ],
  },

  'target_defaults': {
    'defines': [
      # By default, <EGL/eglplatform.h> pulls in some X11 headers that have some
      # nasty macros (|Status|, for example) that conflict with Chromium base.
      #'MESA_EGL_NO_X11_HEADERS'
    ],
    'target_conditions': [
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
      ['sb_pedantic_warnings==1', {
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
          '-Wno-undefined-var-template',
        ],
      }],
    ],
  }, # end of target_defaults
}
