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

# Platform specific configuration for Android on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#
{
  'variables': {
    'target_os': 'android',
    'final_executable_type': 'shared_library',
    'gtest_target_type': 'shared_library',

    'gl_type': 'system_gles2',

    'javascript_engine': 'v8',
    'cobalt_enable_jit': 1,

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',

      # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
      '-ffunction-sections',
      '-funwind-tables',
      '-fstack-protector-strong',
      '-no-canonical-prefixes',
    ],
    'linker_flags': [
      '-static-libstdc++',

      # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
      '-Wl,--build-id',
      '-Wl,--warn-shared-textrel',
      '-Wl,--fatal-warnings',
      '-Wl,--gc-sections',
      '-Wl,-z,nocopyreloc',
    ],
    'compiler_flags_debug': [
      '-frtti',
      '-O0',
    ],
    'compiler_flags_devel': [
      '-frtti',
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
    'platform_libraries': [
      '-lEGL',
      '-lGLESv2',
      '-lOpenSLES',
      '-landroid',
      '-llog',
      '-lmediandk',
    ],
    'conditions': [
      ['clang==1', {
        'common_clang_flags': [
          '-Werror',
          '-fno-exceptions',
          '-fcolor-diagnostics',
          '-fno-strict-aliasing',  # See http://crbug.com/32204
          # Default visibility to hidden, to enable dead stripping.
          '-fvisibility=hidden',
          # Warn for implicit type conversions that may change a value.
          '-Wconversion',
          # Don't warn about register variables (in base and net)
          '-Wno-deprecated-register',
          # Don't warn about deprecated ICU methods (in googleurl and net)
          '-Wno-deprecated-declarations',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          # Don't warn for implicit sign conversions.
          '-Wno-sign-conversion',
          # Triggered by the COMPILE_ASSERT macro.
          '-Wno-unused-local-typedef',
          # Don't warn if a function or variable cannot be implicitly
          # instantiated.
          '-Wno-undefined-var-template',
          # Don't warn about DCHECKs comparing against max int in mozjs-45.
          '-Wno-tautological-constant-out-of-range-compare',
          # Don't warn about comparing unsigned value < 0 in mozjs-45.
          '-Wno-tautological-compare',
          # Don't warn about undefined inlines in mozjs-45.
          '-Wno-undefined-inline',

          # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
          '-fno-limit-debug-info',
        ],
      }],
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
    ],
  },

  'target_defaults': {
    'defines': [
      # Cobalt on Linux flag
      'COBALT_LINUX',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
      # Enable compile-time decisions based on the ABI
      'ANDROID_ABI=<(ANDROID_ABI)',
      # Note -DANDROID is an argument to some ifdefs in the NDK's eglplatform.h
      'ANDROID',
      # Undefining __linux__ causes the system headers to make wrong
      # assumptions about which C-library is used on the platform.
      '__BIONIC__',
      # Undefining __linux__ leaves libc++ without a threads implementation.
      # TODO: See if there's a way to make libcpp threading use Starboard.
      '_LIBCPP_HAS_THREAD_API_PTHREAD',
    ],
    'cflags': [
      # libwebp uses the cpufeatures library to detect ARM NEON support
      '-I<(NDK_HOME)/sources/android/cpufeatures',
    ],
    'cflags_cc': [
      '-std=c++11',
    ],
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          '<@(common_clang_flags)',
        ],
      },{
        'cflags': [
          '<@(common_clang_flags)',
          # 'this' pointer cannot be NULL...pointer may be assumed
          # to always convert to true.
          '-Wno-undefined-bool-conversion',
          # Skia doesn't use overrides.
          '-Wno-inconsistent-missing-override',
          # Do not warn about unused function params.
          '-Wno-unused-parameter',
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
          # shifting a negative signed value is undefined
          '-Wno-shift-negative-value',
          # Width of bit-field exceeds width of its type- value will be truncated
          '-Wno-bitfield-width',
          # Do not warn about an implicit exception spec mismatch.  This is
          # safe, since we do not enable exceptions.
          '-Wno-implicit-exception-spec-mismatch',
        ],
      }],
      ['_type=="executable"', {
        # Android Lollipop+ requires relocatable executables.
        'cflags': [
          '-fPIE',
        ],
        'ldflags': [
          '-pie',
        ],
      },{
        # Android requires relocatable shared libraries.
        'cflags': [
          '-fPIC',
        ],
      }],
      ['use_asan==1', {
        'cflags': [
          '-fsanitize=address',
          '-fno-omit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=address',
          # Force linking of the helpers in sanitizer_options.cc
          '-Wl,-u_sanitizer_options_link_helper',
        ],
        'defines': [
          'ADDRESS_SANITIZER',
        ],
      }],
      ['use_tsan==1', {
        'cflags': [
          '-fsanitize=thread',
          '-fno-omit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=thread',
        ],
        'defines': [
          'THREAD_SANITIZER',
        ],
      }],
    ],
  }, # end of target_defaults
}
