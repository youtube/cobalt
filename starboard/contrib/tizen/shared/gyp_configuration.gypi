# Copyright 2016 Samsung Electronics. All Rights Reserved.
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
    'target_os': 'linux',
    'tizen_os': 1,

    # Some package use tizen system instead of third_party
    'use_system_icu': 1,
    'use_system_libxml': 1,

    # scratch surface cache is designed to choose large offscreen surfaces so
    # that they can be maximally reused, it is not a very good fit for a tiled
    # renderer.
    'scratch_surface_cache_size_in_bytes' : 0,

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'mozjs',
    'cobalt_enable_jit': 0,

    'linker_flags': [
    ],
    'linker_flags_gold': [
      '-O3',
      '-flto',
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_qa': [
      '-O3',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-O3',
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
          '-flto',
        ],
      }],
    ],
  },

  'target_defaults': {
    'defines': [
      # Cobalt on Tizen flag
      'COBALT_TIZEN',
      'PNG_SKIP_SETJMP_CHECK',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
    ],
    'cflags': [
      '-pthread',
      # Force char to be signed.
      '-fsigned-char',
      # Do not warn about locally defined but not used.
      '-Wno-unused-local-typedefs',
      # Do not warn about XXX is deprecated.
      '-Wno-deprecated-declarations',
      # Do not warn about missing initializer for member XXX.
      '-Wno-missing-field-initializers',
      # Do not warn about unused functions.
      '-Wno-unused-function',
      # Do not warn about type qualifiers ignored on function return type.
      '-Wno-ignored-qualifiers',
      # Do not warn about the use of multi-line comments.
      '-Wno-comment',
      # Do not warn about sign compares.
      '-Wno-sign-compare',
    ],
    'cflags_c': [
      '-std=c11',
    ],
    'cflags_cc': [
      '-std=gnu++11',
    ],
    'ldflags': [
      '-pthread',
    ],
  }, # end of target_defaults
}
