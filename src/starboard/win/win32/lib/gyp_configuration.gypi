# Copyright 2017 Google Inc. All Rights Reserved.
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
    # TODO: In theory, there are tools that can combine static libraries into
    # thick static libraries with all their transitive dependencies. Using
    # shared_library here can have unexpected consequences. Explore building
    # this into a thick static library instead.
    'final_executable_type': 'shared_library',
    'default_renderer_options_dependency': '<(DEPTH)/cobalt/renderer/rasterizer/lib/lib.gyp:external_rasterizer',
    'sb_enable_lib': 1,

    # We turn off V-Sync and throttling in Cobalt so we can let the client
    # decide if they want those features.
    'cobalt_egl_swap_interval': 0,
    'cobalt_minimum_frame_time_in_milliseconds': 0,

    'enable_map_to_mesh': 1,
    'angle_build_winrt': 0,
    'winrt': 0,
    'enable_d3d11_feature_level_11': 1,
    'cobalt_user_on_exit_strategy': 'noexit',
  },
  'includes': [
    '../../shared/gyp_configuration.gypi',
  ],
  'target_defaults': {
    'default_configuration': 'win-win32-lib_debug',
    'configurations': {

      'win-win32-lib_debug': {
        'inherit_from': ['win32_base', 'msvs_debug'],
      },
      'win-win32-lib_devel': {
       'inherit_from': ['win32_base', 'msvs_devel'],
      },
      'win-win32-lib_qa': {
        'inherit_from': ['win32_base', 'msvs_qa'],
      },
      'win-win32-lib_gold': {
        'inherit_from': ['win32_base', 'msvs_gold'],
      },
    },  # end of configurations
  },
}
