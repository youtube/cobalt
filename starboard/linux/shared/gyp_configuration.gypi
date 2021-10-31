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

    'target_os': 'linux',
    'yasm_exists': 1,
    'sb_widevine_platform' : 'linux',
    'sb_disable_opus_sse': 1,
    'sb_enable_benchmark': 1,

    'platform_libraries': [
      '-lasound',
      '-ldl',
      '-lpthread',
      '-lrt',
    ],

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
    'conditions': [
      ['cobalt_config == "debug"', {
        'defines': [
          # Enable debug mode for the C++ standard library.
          # https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/manual/debug_mode_using.html
          # https://libcxx.llvm.org/docs/DesignDocs/DebugMode.html
          '_GLIBCXX_DEBUG',
          '_LIBCPP_DEBUG=1',
        ],
      }],
      ['cobalt_config == "devel"', {
        'defines': [
          # Enable debug mode for the C++ standard library.
          # https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/manual/debug_mode_using.html
          # https://libcxx.llvm.org/docs/DesignDocs/DebugMode.html
          '_GLIBCXX_DEBUG',
          '_LIBCPP_DEBUG=0',
        ],
      }],
    ],
  }, # end of target_defaults
}
