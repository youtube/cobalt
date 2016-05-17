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

{
  'variables': {
    'target_arch': 'arm',
    'target_os': 'linux',

    'enable_webdriver': '1',
    'in_app_dial%': 0,
    'sysroot%': '/',
    'gl_type': 'system_gles2',

    # This should have a default value in cobalt/base.gypi. See the comment
    # there for acceptable values for this variable.
    'javascript_engine': 'javascriptcore',

    # RasPi 1 is ARMv6
    'arm_version': 6,
    'armv7': 0,
    'arm_neon': 0,

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',

      # Suppress some warnings that will be hard to fix.
      '-Wno-unused-local-typedefs',
      '-Wno-unused-result',
      '-Wno-deprecated-declarations',
      '-Wno-missing-field-initializers',
      '-Wno-comment',  # Talk to my lawyer.
      '-Wno-narrowing',
      '-Wno-unknown-pragmas',
      '-Wno-type-limits',  # TODO: We should actually look into these.

      # Specify the sysroot with all your include dependencies.
      '--sysroot=<(sysroot)',

      # Optimize for Raspberry Pi 1 chips.
      '-march=armv6zk',
      '-mcpu=arm1176jzf-s',
      '-mfloat-abi=hard',
      '-mfpu=vfp',

      # This is a quirk of Raspbian, these are required to include any
      # GL-related headers.
      '-I<(sysroot)/opt/vc/include',
      '-I<(sysroot)/opt/vc/include/interface/vcos/pthreads',
      '-I<(sysroot)/opt/vc/include/interface/vmcs_host/linux',
    ],
    'linker_flags': [
      '--sysroot=<(sysroot)',
      # This is a quirk of Raspbian, these are required to link any GL-related
      # libraries.
      '-L<(sysroot)/opt/vc/lib',
      '-Wl,-rpath=<(sysroot)/opt/vc/lib',

      # We don't wrap these symbols, but this ensures that they aren't
      # linked in.
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
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_cc_debug': [
      '-frtti',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_devel': [
      '-frtti',
    ],
    'compiler_flags_qa': [
      '-O2',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-O2',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavresample',
      '-lavutil',
      '-lEGL',
      '-lGLESv2',
      '-lpthread',
      '-lpulse',
      '-lrt',
      '-lopenmaxil',
      '-lbcm_host',
      '-lvcos',
      '-lvchiq_arm',
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
    'defines': [
      # Cobalt on Linux flag
      'COBALT_LINUX',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
    ],
    'cflags_c': [
      '-std=c11',
    ],
    'cflags_cc': [
      '-std=gnu++11',
      '-Wno-literal-suffix',
    ],
    'default_configuration': 'raspi-1_debug',
    'configurations': {
      'raspi-1_debug': {
        'inherit_from': ['debug_base'],
      },
      'raspi-1_devel': {
        'inherit_from': ['devel_base'],
      },
      'raspi-1_qa': {
        'inherit_from': ['qa_base'],
      },
      'raspi-1_gold': {
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
