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
      'target_name': 'cxx',
      'type': 'static_library',
      'include_dirs': [
        'include',
        '<(DEPTH)/third_party/llvm-project/libcxxabi/include',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/common/common.gyp:common',
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
        '<(DEPTH)/third_party/musl/musl.gyp:c',
        '<(DEPTH)/third_party/llvm-project/compiler-rt/compiler-rt.gyp:compiler_rt',
        '<(DEPTH)/third_party/llvm-project/libcxxabi/libcxxabi.gyp:cxxabi',
        '<(DEPTH)/third_party/llvm-project/libunwind/libunwind.gyp:unwind_evergreen',
      ],
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
      'defines': [
        '_LIBCPP_BUILDING_LIBRARY',
        '_LIBCPP_HAS_MUSL_LIBC',
        '_LIBCPP_HAS_NO_STDIN',
        # This macro is used to build libcxxabi with libunwind.
        'LIBCXX_BUILDING_LIBCXXABI',
      ],
      'sources': [
        'include/apple_availability.h',
        'include/atomic_support.h',
        'include/config_elast.h',
        'include/refstring.h',
        'src/algorithm.cpp',
        'src/any.cpp',
        'src/bind.cpp',
        'src/charconv.cpp',
        'src/chrono.cpp',
        'src/condition_variable.cpp',
        'src/exception.cpp',

        # Remove file system operations
        #'src/filesystem/directory_iterator.cpp',
        #'src/filesystem/filesystem_common.h',
        #'src/filesystem/int128_builtins.cpp',
        #'src/filesystem/operations.cpp',

        'src/functional.cpp',
        'src/future.cpp',
        'src/hash.cpp',
        'src/ios.cpp',
        'src/iostream.cpp',
        'src/locale.cpp',
        'src/memory.cpp',
        'src/mutex.cpp',
        'src/new.cpp',
        'src/optional.cpp',
        'src/regex.cpp',
        'src/shared_mutex.cpp',
        'src/stdexcept.cpp',
        'src/string.cpp',
        'src/strstream.cpp',
        'src/system_error.cpp',
        'src/thread.cpp',
        'src/typeinfo.cpp',
        'src/utility.cpp',
        'src/valarray.cpp',
        'src/variant.cpp',
        'src/vector.cpp',
      ],
    }
  ]
}
