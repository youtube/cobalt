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
    'target_arch': 'mips',
    'target_os': 'linux',

    'cobalt_media_source_2016': 1,
    'in_app_dial%': 0,
    'rasterizer_type': 'direct-gles',
    'scratch_surface_cache_size_in_bytes' : 0,

    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavutil',
      '-lm',
      '-lpthread',
      '-lrt',
    ],

    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',
      '--sysroot=<(sysroot)',
      '-EL',
    ],

    'linker_flags': [
      '--sysroot=<(sysroot)',
      '-EL',

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
      '-Wl,--wrap=eglGetDisplay',
      '-Wl,--wrap=eglTerminate',
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
