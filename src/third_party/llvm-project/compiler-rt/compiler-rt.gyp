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

{
  'targets': [
    {
      'target_name': 'compiler_rt',
      'type': 'static_library',
      'cflags': [
        '-nostdinc',
      ],
      'cflags_cc': [
        '-std=c++11',
        '-nostdinc++',
        '-fvisibility-inlines-hidden',
        '-fms-compatibility-version=19.00',
        '-Wall',
        '-Wextra',
        '-W',
        '-Wwrite-strings',
        '-Wno-long-long',
        '-Werror=return-type',
        '-Wno-user-defined-literals',
        '-Wno-bitwise-op-parentheses',
        '-Wno-shift-op-parentheses',
        '-Wno-error',
        '-Wno-unused-parameter',
        '-Wno-unused-command-line-argument',
        '-fPIC',
      ],
      'sources': [
        'lib/builtins/udivti3.c',
        'lib/builtins/udivmodti4.c',
        'lib/builtins/umodti3.c',
      ],
      'conditions': [
        ['sb_evergreen == 1 and target_arch == "arm"', {
          'sources': [
            'lib/builtins/divdi3.c',
            'lib/builtins/divmoddi4.c',
            'lib/builtins/divmodsi4.c',
            'lib/builtins/divsi3.c',
            'lib/builtins/fixdfdi.c',
            'lib/builtins/fixsfdi.c',
            'lib/builtins/fixunsdfdi.c',
            'lib/builtins/fixunssfdi.c',
            'lib/builtins/floatdidf.c',
            'lib/builtins/floatdisf.c',
            'lib/builtins/floatundidf.c',
            'lib/builtins/floatundisf.c',
            'lib/builtins/udivmoddi4.c',
            'lib/builtins/udivmodsi4.c',
            'lib/builtins/udivsi3.c',

            'lib/builtins/arm/aeabi_idivmod.S',
            'lib/builtins/arm/aeabi_ldivmod.S',
            'lib/builtins/arm/aeabi_memcmp.S',
            'lib/builtins/arm/aeabi_memcpy.S',
            'lib/builtins/arm/aeabi_memmove.S',
            'lib/builtins/arm/aeabi_memset.S',
            'lib/builtins/arm/aeabi_uidivmod.S',
            'lib/builtins/arm/aeabi_uldivmod.S',
          ],
        }],
        ['sb_evergreen == 1 and target_arch == "arm64"', {
          'sources': [
            'lib/builtins/addtf3.c',
            'lib/builtins/comparetf2.c',
            'lib/builtins/divtf3.c',
            'lib/builtins/extenddftf2.c',
            'lib/builtins/extendsftf2.c',
            'lib/builtins/fixtfsi.c',
            'lib/builtins/floatsitf.c',
            'lib/builtins/floatunsitf.c',
            'lib/builtins/multf3.c',
            'lib/builtins/subtf3.c',
            'lib/builtins/trunctfdf2.c',
            'lib/builtins/trunctfsf2.c',
          ],
        }],
        ['sb_evergreen == 1 and target_arch == "x86"', {
          'sources': [
            'lib/builtins/i386/divdi3.S',
            'lib/builtins/i386/moddi3.S',
            'lib/builtins/i386/udivdi3.S',
            'lib/builtins/i386/umoddi3.S',
          ],
        }],
      ]
    }
  ]
}
