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
  'targets': [
    {
      'target_name': 'common',
      'type': 'static_library',

      'sources': [
        'find_node.cc',
        'find_node.h',
        'offscreen_render_coordinate_mapping.cc',
        'offscreen_render_coordinate_mapping.h',
        'scratch_surface_cache.cc',
        'scratch_surface_cache.h',
        'streaming_best_fit_line.cc',
        'streaming_best_fit_line.h',
        'utils.h',
        'utils.cc',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
      ],
    },
  ],
}
