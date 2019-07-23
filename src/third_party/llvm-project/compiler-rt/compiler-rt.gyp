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
    }
  ]
}