# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
    # Override that omits the "data" subdirectory.
    # TODO: Remove when omitted for all platforms in base_configuration.gypi.
    'sb_static_contents_output_data_dir': '<(PRODUCT_DIR)/content',

    'target_arch%': 'x64',
    'target_os': 'linux',
    'yasm_exists': 1,
    'sb_widevine_platform' : 'linux',

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
    'conditions': [
        ['sb_evergreen != 1', {
          # TODO: allow starboard_platform to use system libc/libc++ in the
          # future. For now, if this flags is enabled, a warning emerge saying
          # it's unused anyway.
          'linker_flags': [
            '-static-libstdc++',
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

  'includes': [
    '<(DEPTH)/starboard/sabi/sabi.gypi',
  ],
}
