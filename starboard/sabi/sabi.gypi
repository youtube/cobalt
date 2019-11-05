# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

# Starboard Application Binary Interface
#
# This file is [mostly] platform-agnostic and translates the set of ABI
# variables, which should be overridden by each Evergreen platform, into build
# time defines.

{
  'variables': {
    # Default ABI variables
    'target_arch%': '',
    'target_arch_sub%': '',
    'word_size%': 0,
    'endianness%': '',
    'calling_convention%': '',
    'floating_point_abi%': '',
    'floating_point_fpu%': '',
    'signedness_of_char%': '',
    'alignment_char%':    0,
    'alignment_double%':  0,
    'alignment_float%':   0,
    'alignment_int%':     0,
    'alignment_llong%':   0,
    'alignment_long%':    0,
    'alignment_pointer%': 0,
    'alignment_short%':   0,
    'alignment_stack%': 0,
    'size_of_char%':    0,
    'size_of_double%':  0,
    'size_of_float%':   0,
    'size_of_int%':     0,
    'size_of_llong%':   0,
    'size_of_long%':    0,
    'size_of_pointer%': 0,
    'size_of_short%':   0,

    # arm and arm64
    'conditions': [
      ['target_arch=="arm" or target_arch=="arm64"', {
        'arm_float_abi': '<(floating_point_abi)',
        'arm_fpu': '<(floating_point_fpu)',

        'conditions': [
          ['target_arch_sub=="v6zk"', {
            'arm_version': 6,
            'armv7': 0,
          }],
          ['target_arch_sub=="v7a"', {
            'arm_version': 7,
            'armv7': 1,
          }],
          ['target_arch_sub=="v8a"', {
            'arm_version': 8,
            'armv7': 0,
          }],
        ],
      }],
    ],
  },
  'target_defaults': {
    'defines': [
      # Inlined Python used to capitalize the variable values.
      'SB_IS_ARCH_<!(python -c "print(\'<(target_arch)\'.upper())")=1',
      'SB_HAS_<!(python -c "print(\'<(calling_convention)\'.upper())")_CALLING=1',
      'SB_HAS_<!(python -c "print(\'<(floating_point_abi)\'.upper())")_FLOATS=1',

      'SB_IS_<(word_size)_BIT=1',
      'SB_ALIGNMENT_OF_CHAR=<(alignment_char)',
      'SB_ALIGNMENT_OF_DOUBLE=<(alignment_double)',
      'SB_ALIGNMENT_OF_FLOAT=<(alignment_float)',
      'SB_ALIGNMENT_OF_INT=<(alignment_int)',
      'SB_ALIGNMENT_OF_LLONG=<(alignment_llong)',
      'SB_ALIGNMENT_OF_LONG=<(alignment_long)',
      'SB_ALIGNMENT_OF_POINTER=<(alignment_pointer)',
      'SB_ALIGNMENT_OF_SHORT=<(alignment_short)',
      'SB_ALIGNMENT_OF_STACK=<(alignment_stack)',
      'SB_SIZE_OF_CHAR=<(size_of_char)',
      'SB_SIZE_OF_DOUBLE=<(size_of_double)',
      'SB_SIZE_OF_FLOAT=<(size_of_float)',
      'SB_SIZE_OF_INT=<(size_of_int)',
      'SB_SIZE_OF_LONG=<(size_of_long)',
      'SB_SIZE_OF_LLONG=<(size_of_llong)',
      'SB_SIZE_OF_POINTER=<(size_of_pointer)',
      'SB_SIZE_OF_SHORT=<(size_of_short)',
    ],
    'conditions': [
      ['endianness=="little"', {
        'defines': ['SB_IS_BIG_ENDIAN=0'],
      }, {
        'defines': ['SB_IS_BIG_ENDIAN=1'],
      }],
      ['signedness_of_char=="signed"', {
        'defines': ['SB_HAS_SIGNED_CHAR=1'],
      }, {
        'defines': ['SB_HAS_SIGNED_CHAR=0'],
      }],
      # TODO: Remove when all platforms have adopted the ABI manifest.
      ['size_of_pointer==4', {
        'defines': ['SB_HAS_32_BIT_POINTERS=1'],
      }, {
        'defines': ['SB_HAS_64_BIT_POINTERS=1'],
      }],
      # TODO: Remove when all platforms have adopted the ABI manifest.
      ['size_of_long==4', {
        'defines': ['SB_HAS_32_BIT_LONG=1'],
      }, {
        'defines': ['SB_HAS_64_BIT_LONG=1'],
      }],
      # TODO: Remove when all platforms have adopted the ABI manifest.
      ['target_arch=="arm64"', {
        'defines': ['SB_IS_ARCH_ARM=1'],
      }],
      # TODO: Remove when all platforms have adopted the ABI manifest.
      ['target_arch=="x64" or target_arch=="ia32"', {
        'defines': ['SB_IS_ARCH_X86'],
      }],
    ],
  },
  'includes': [
    # This JSON file also happens to be valid GYP configuration language so we
    # include it here. Slightly hacky, highly effective.
    '<(DEPTH)/<(sabi_json_path)',
  ],
}

