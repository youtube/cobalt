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

{
  'variables': {
    'cobalt_code': 1,
  },

  'targets': [
    # Compiles code related to the definition of the render tree.  The render
    # tree is the output of the layout stage and the input to the rendering
    # stage.
    {
      'target_name': 'render_tree',
      'type': 'static_library',
      'sources': [
        'border.cc',
        'border.h',
        'brush.cc',
        'brush.h',
        'brush_visitor.h',
        'color_rgba.h',
        'composition_node.cc',
        'composition_node.h',
        'filter_node.cc',
        'filter_node.h',
        'font.h',
        'font_fallback_list.h',
        'glyph.h',
        'glyph_buffer.h',
        'image_node.cc',
        'image_node.h',
        'matrix_transform_node.cc',
        'matrix_transform_node.h',
        'node.h',
        'node_visitor.h',
        'opacity_filter.h',
        'rect_node.cc',
        'rect_node.h',
        'rect_shadow_node.cc',
        'rect_shadow_node.h',
        'resource_provider.h',
        'resource_provider_stub.h',
        'rounded_corners.h',
        'rounded_viewport_filter.h',
        'shadow.h',
        'text_node.cc',
        'text_node.h',
        'typeface.h',
        'viewport_filter.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
      ],
    },

    # Support for describing and applying render tree animations.
    {
      'target_name': 'animations',
      'type': 'static_library',
      'sources': [
        'animations/node_animations_map.cc',
        'animations/node_animations_map.h',
      ],
      'dependencies': [
        'render_tree',
      ],
    },

    # Tests the render tree utility functionality.
    {
      'target_name': 'render_tree_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'animations/node_animations_map_test.cc',
        'brush_visitor_test.cc',
        'color_rgba_test.cc',
        'node_visitor_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'animations',
        'render_tree',
      ],
    },

    # Deploys the render tree library test on a console.
    {
      'target_name': 'render_tree_test_deploy',
      'type': 'none',
      'dependencies': [
        'render_tree_test',
      ],
      'variables': {
        'executable_name': 'render_tree_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
