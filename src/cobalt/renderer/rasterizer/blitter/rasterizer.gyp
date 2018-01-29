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
    # Code shared by both the hardware and software Blitter rasterizers.
    {
      'target_name': 'common',
      'type': 'static_library',

      'sources': [
        'skia_blitter_conversions.cc',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/software_rasterizer.gyp:software_rasterizer',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
    # The Blitter hardware rasterizer uses the Blitter API to draw render tree
    # nodes directly, when possible, and falls back to software Skia when this
    # is not possible.
    {
      'target_name': 'hardware_rasterizer',
      'type': 'static_library',

      'sources': [
        'cached_software_rasterizer.cc',
        'hardware_rasterizer.cc',
        'image.cc',
        'linear_gradient.cc',
        'linear_gradient_cache.cc',
        'render_state.cc',
        'render_tree_blitter_conversions.cc',
        'render_tree_node_visitor.cc',
        'resource_provider.cc',
        'scratch_surface_cache.cc',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/cobalt/renderer/rasterizer/common/common.gyp:common',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/software_rasterizer.gyp:software_rasterizer',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        'common',
      ],

      'conditions': [
        ['render_dirty_region_only==1', {
          'defines': [
            'COBALT_RENDER_DIRTY_REGION_ONLY',
          ],
        }],
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
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/software_rasterizer.gyp:software_rasterizer',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        'common',
      ],
    },

    {
      'target_name': 'blitter_rasterizer_tests',
      'type': '<(gtest_target_type)',

      'sources': [
        'linear_gradient_cache_test.cc',
      ],

      'dependencies': [
        'hardware_rasterizer',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest'
      ],
    },

    {
      'target_name': 'blitter_rasterizer_tests_deploy',
      'type': 'none',
      'dependencies': [
        'blitter_rasterizer_tests',
      ],
      'variables': {
        'executable_name': 'blitter_rasterizer_tests',
      },
      'includes': [ '../../../../starboard/build/deploy.gypi' ],
    },
  ],
}
