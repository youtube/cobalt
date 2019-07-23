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
  'cflags!': [
    # This flag makes the compilier complain warnings as errors.
    '-Werror',
  ],
  'cflags': [
    # We use -nostdlibinc instead of -nostdinc because even though we don't
    # want the compiler to search the standard system directories for include
    # files, we still want it to search in clang's builtin directories for
    # include files.
    '-nostdlibinc',
    '-Wno-conversion',
    '-Wno-bitwise-op-parentheses',
    '-Wno-shift-op-parentheses',
    '-Wno-shorten-64-to-32',
    '-Wno-unused-command-line-argument',
  ],
  'cflags_cc': [
    '-nostdinc++',
    '-std=c++11',
  ],
  'ldflags': [
    '-stdlib=libc++',
    '-nostdlib',
  ],
  'defines' : [
    # This flag is set assuming the system uses pthread.
    '_LIBCPP_HAS_THREAD_API_PTHREAD',
    '_LIBCPP_HAS_MUSL_LIBC',
  ],
}
