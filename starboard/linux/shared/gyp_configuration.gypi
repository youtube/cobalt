# Copyright 2014 Google Inc. All Rights Reserved.
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

# Platform specific configuration for Linux on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#
{
  'variables': {
    'target_arch%': 'x64',
    'target_os': 'linux',

    'platform_libraries': [
      '-lasound',
      '-ldl',
      '-lpthread',
      '-lrt',
    ],

    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',
    ],

    # Using an inner scope for 'variables' so that it can be made a default
    # (and so overridden elsewhere), but yet still used immediately in this
    # file.
    'variables': {
      'use_dlmalloc_allocator%': 0,
    },
    'linker_flags': [
      '-static-libstdc++'
    ],

    'conditions': [
      ['use_dlmalloc_allocator==1 and use_asan==0', {
        'linker_flags': [
          # If we're not using the system allocator (e.g. we are using dlmalloc
          # and ASAN is inactive) then we should never be making any calls to
          # malloc() or free().  The following linker flags ensure that they
          # are not linked in because we don't actually implement the wrapped
          # version of them. We do link them in when using ASAN, as it needs to
          # use its own version of these allocators in the Starboard
          # implementation.
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
  }, # end of target_defaults
}
