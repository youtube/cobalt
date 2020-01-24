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
      'target_name': 'cxxabi',
      'type': 'static_library',
      'include_dirs': [
        'include',
        '<(DEPTH)/third_party/llvm-project/libunwind/include',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/musl/musl.gyp:c',
        '<(DEPTH)/third_party/llvm-project/libunwind/libunwind.gyp:unwind_evergreen',
      ],
      'cflags': [
        '-nostdinc',
      ],
      'cflags_cc': [
        '-std=c++11',
        '-nostdinc++',
        '-fPIC',
        '-fexceptions',
        '-frtti',
        '-Werror=return-type',
        '-fvisibility-inlines-hidden',
        '-fms-compatibility-version=19.00',
        '-funwind-tables',
        '-W',
        '-Wall',
        '-Wextra',
        '-Wwrite-strings',
        '-Wshorten-64-to-32',
        '-Wno-error',
        '-Wno-unused-command-line-argument',
      ],
      'defines' : [
        # This macro is used to disable extern template declarations in the libc++
        # headers. The intended use case is for clients who wish to use the libc++
        # headers without taking a dependency on the libc++ library itself.
        '_LIBCPP_DISABLE_EXTERN_TEMPLATE',
        # This macro is used to re-enable the `set_unexpected`, `get_unexpected`, and
        # `unexpected` functions, which were removed in C++17.
        '_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS',
        # Let the library headers know they are currently being used to build the
        # library.
        '_LIBCXXABI_BUILDING_LIBRARY',
        'LIBCXXABI_USE_LLVM_UNWINDER',
      ],
      'sources': [
        'src/abort_message.cpp',
        'src/abort_message.h',
        'src/cxa_aux_runtime.cpp',
        'src/cxa_default_handlers.cpp',
        'src/cxa_demangle.cpp',
        'src/cxa_exception.cpp',
        'src/cxa_exception.hpp',
        'src/cxa_exception_storage.cpp',
        'src/cxa_guard.cpp',
        'src/cxa_handlers.cpp',
        'src/cxa_handlers.hpp',
        'src/cxa_personality.cpp',
        'src/cxa_unexpected.cpp',
        'src/cxa_vector.cpp',
        'src/cxa_virtual.cpp',
        'src/demangle/Compiler.h',
        'src/demangle/StringView.h',
        'src/demangle/Utility.h',
        'src/fallback_malloc.cpp',
        'src/fallback_malloc.h',
        'src/include/atomic_support.h',
        'src/include/refstring.h',
        'src/private_typeinfo.cpp',
        'src/private_typeinfo.h',
        'src/stdlib_exception.cpp',
        'src/stdlib_new_delete.cpp',
        'src/stdlib_stdexcept.cpp',
        'src/stdlib_typeinfo.cpp',
      ],
      'sources!': [
        # We utilize exception handling and the following file breaks the build.
        'src/cxa_noexception.cpp',

        # Not needed and leaks __cxa_thread_atexit_impl.
        'src/cxa_thread_atexit.cpp',
      ],
      'all_dependent_settings': {
        'defines': [
          'LIBCXXABI_USE_LLVM_UNWINDER',
        ],
      },
    }
  ]
}
