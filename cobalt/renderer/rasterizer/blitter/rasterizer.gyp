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
    # The Blitter hardware rasterizer uses the Blitter API to draw render tree
    # nodes directly, when possible, and falls back to software Skia when this
    # is not possible.
    {
      'target_name': 'hardware_rasterizer',
      'type': 'static_library',

      'sources': [
        'hardware_rasterizer.cc',
        'image.cc',
        'render_tree_blitter_conversions.cc',
        'render_tree_node_visitor.cc',
        'resource_provider.cc',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },

    # The Blitter software rasterizer uses Skia to rasterize render trees on
    # the CPU and then uses the Blitter API to get the resulting image onto
    # the display.
    {
      'target_name': 'software_rasterizer',
      'type': 'static_library',

      'sources': [
        'software_rasterizer.cc',
        'software_rasterizer.h',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/software_rasterizer.gyp:software_rasterizer',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
  ],
}