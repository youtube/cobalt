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
      '-gline-tables-only',
    ],
    'compiler_flags_gold_size': [
      '-Os',
    ],
    'compiler_flags_gold_speed': [
      '-O2',
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
          # Warns on switches on enums that cover all enum values but
          # also contain a default: branch. Chrome is full of that.
          '-Wno-covered-switch-default',
          # protobuf uses hash_map.
          '-Wno-deprecated',
          '-fno-exceptions',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          '-fno-strict-aliasing',  # See http://crbug.com/32204
          # Don't warn for invalid access to non-static data member of NULL object.
          '-Wno-invalid-offsetof',
          # Suppress 'implicit conversion changes signedness'
          '-Wno-sign-conversion',
          # shifting a negative signed value is undefined
          '-Wno-shift-sign-overflow',
          # Suppress "comparison may be assumed to always evaluate to false"
          '-Wno-tautological-undefined-compare',
          # Suppress "comparison of unsigned enum expression < 0 is always false"
          '-Wno-tautological-compare',
          # Suppress "[type1] has C-linkage specified, but returns user-defined type [type2] which is incompatible with C"
          '-Wno-return-type-c-linkage',
          '-Wno-array-bounds',
          # Suppress "template argument uses unnamed type"
          '-Wno-unnamed-type-template-args',
          # 'this' pointer cannot be NULL...pointer may be assumed
          # to always convert to true.
          '-Wno-undefined-bool-conversion',
          # Skia doesn't use overrides.
          '-Wno-inconsistent-missing-override',
          # Triggered by the COMPILE_ASSERT macro.
          '-Wno-unused-local-typedef',
          # Suppress "'&&' within '||'"
          '-Wno-logical-op-parentheses',
          '-Wno-unused-parameter',
          # Suppress warnings about unknown pragmas.
          '-Wno-unknown-pragmas',
          # Suppress warnings about equality checks within double parentheses.
          '-Wno-parentheses-equality',
          # Suppress warnings about unreachable code due to constexpr conditions
          '-Wno-unreachable-code',
          # Suppress warnings about values being written but not read before the next write.
          '-Wno-unused-value',
          # Suppress warnings related to unused compilation flags in clang.
          '-Wno-unused-command-line-argument',
          # Suppress warnings in libjpeg
          '-Wno-shift-negative-value',
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
    'cflags_c': [
      # Limit to C99. This allows Linux to be a canary build for any
      # C11 features that are not supported on some platforms' compilers.
      '-std=c99',
    ],
    'cflags_cc': [
      '-std=c++14',
    ],
    'target_conditions': [
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
          # Do not warn about an implicit exception spec mismatch.
          '-Wno-implicit-exception-spec-mismatch',
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
