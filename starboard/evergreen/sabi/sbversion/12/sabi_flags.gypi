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
# This file translates the set of ABI variables, which should be overridden by
# each platform, into platform-specific sets of compiler flags.

{
  'variables': {
    'conditions': [
      # arm
      ['target_arch=="arm"', {
        'compiler_flags': [
          '-march=arm<(target_arch_sub)',
        ],
        'conditions': [
          ['floating_point_abi=="soft"', {
            'compiler_flags': [
              '-mfloat-abi=soft',
            ],
          }, {
            'compiler_flags': [
              '-mfloat-abi=<(floating_point_abi)',
              '-mfpu=<(floating_point_fpu)',
            ],
          }],
          ['floating_point_abi=="hard"', {
            'compiler_flags': [
              '-target', 'arm<(target_arch_sub)-none-eabihf',
            ],
          }, {
            'compiler_flags': [
              '-target', 'arm<(target_arch_sub)-none-eabi',
            ],
          }],
        ],
      }],
      # arm64
      ['target_arch=="arm64"', {
        'compiler_flags': [
          '-march=arm64<(target_arch_sub)',
          '-target', 'arm64<(target_arch_sub)-unknown-unknown-elf',

          # There is no software floating point support for arm64 / aarch64
          # architectures. Thus, -mfloat-abi and -mfpu would simply be ignored.

          # char is unsigned by default for arm64.
          '-fsigned-char',
        ],
      }],
      # x86
      ['target_arch=="x86"', {
        'compiler_flags': [
          '-march=i686',
          '-target', 'i686-unknown-unknown-elf',
        ],
      }],
      # x64
      ['target_arch=="x64"', {
        'compiler_flags': [
          '-march=x86-64',
          '-target', 'x86_64-unknown-linux-elf',
        ],
      }],
    ],
  },
}

