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
      '-Wl,-rpath=<@(toolchain_lib_path)',
    ],
    'compiler_flags_cc_debug': [
      '-frtti',
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_cc_devel': [
      '-frtti',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_qa': [
      '-O2',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-O2',
    ],
    'common_compiler_flags': [
      # Default visibility to hidden, to enable dead stripping.
      '-fvisibility=hidden',
      # protobuf uses hash_map.
      '-fno-exceptions',
      # Don't warn about the "struct foo f = {0};" initialization pattern.
      '-Wno-missing-field-initializers',
      '-fno-strict-aliasing',  # See http://crbug.com/32204
      # Don't warn about any conversions.
      '-Wno-conversion',
      # Don't warn about unreachable code. See
      # http://gcc.gnu.org/bugzilla/show_bug.cgi?id=46158
      '-Wno-unreachable-code',
      '-Wno-deprecated-declarations',
      # Disable warning:
      # 'comparison is always true due to limited range of data type'
      '-Wno-extra',
      # Don't warn about inlining
      '-Wno-inline',
      # Disable warning: 'typedef locally defined but not used'.
      '-Wno-unused-local-typedefs',
      # Disable warning: 'narrowing conversion'
      '-Wno-narrowing',
      # Do not remove null this checks.
      '-fno-delete-null-pointer-checks',
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
    'cflags_c': [
      # Limit to C99. This allows Linux to be a canary build for any
      # C11 features that are not supported on some platforms' compilers.
      '-std=c99',
    ],
    'cflags_cc': [
      '-std=gnu++11',
      # Don't warn for invalid access to non-static data member of NULL object.
      '-Wno-invalid-offsetof',
      # Don't warn about deprecated use
      '-Wno-deprecated',
    ],
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '<@(common_compiler_flags)',
        ],
      },{
        'cflags': [
          '<@(common_compiler_flags)',
          # Do not warn about unused function params.
          '-Wno-unused-parameter',
        ],
      }],
    ],
  }, # end of target_defaults
}
