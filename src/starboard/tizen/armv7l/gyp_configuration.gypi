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
    'target_arch': 'arm',
    'target_os': 'tizen',

    'gl_type': 'system_gles2',

    # Tizen TV is ARMv7
    'arm_version': 7,
    'armv7': 1,
    'arm_neon': 0,

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'javascriptcore',

    'platform_libraries': [
    	'-lasound',
    ],
    'linker_flags': [
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_devel': [
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
      # Cobalt on Tizen flag
      'COBALT_TIZEN',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
    ],
    'cflags': [
      '-pthread',
      # Do not warn about locally defined but not used.
      '-Wno-unused-local-typedefs',
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
    'default_configuration': 'tizen-armv7l_debug',
    'configurations': {
      'tizen-armv7l_debug': {
        'inherit_from': ['debug_base'],
      },
      'tizen-armv7l_devel': {
        'inherit_from': ['devel_base'],
      },
      'tizen-armv7l_qa': {
        'inherit_from': ['qa_base'],
      },
      'tizen-armv7l_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
    'target_conditions': [
      ['cobalt_code==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
        ],
      },{
        'cflags': [
          # Do not warn about unused function params.
          '-Wno-unused-parameter',
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
        ],
      }],
    ],
  }, # end of target_defaults
}
