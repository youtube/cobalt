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
    'sb_pedantic_warnings': 1,
  },

  'targets': [
    # Compiles code related to the definition of the render tree.  The render
    # tree is the output of the layout stage and the input to the rendering
    # stage.
    {
      'target_name': 'render_tree',
      'type': 'static_library',
      'sources': [
        'blur_filter.h',
        'border.cc',
        'border.h',
        'brush.cc',
        'brush.h',
        'brush_visitor.h',
        'child_iterator.h',
        'color_rgba.h',
        'composition_node.cc',
        'composition_node.h',
        'dump_render_tree_to_string.cc',
        'filter_node.cc',
        'filter_node.h',
        'font.h',
        'font_provider.h',
        'glyph.h',
        'glyph_buffer.h',
        'image_node.cc',
        'image_node.h',
        'map_to_mesh_filter.h',
        'matrix_transform_3d_node.cc',
        'matrix_transform_3d_node.h',
        'matrix_transform_node.cc',
        'matrix_transform_node.h',
        'mesh.h',
        'node.h',
        'node.cc',
        'node_visitor.h',
        'opacity_filter.h',
        'punch_through_video_node.cc',
        'rect_node.cc',
        'rect_node.h',
        'rect_shadow_node.cc',
        'rect_shadow_node.h',
        'resource_provider.h',
        'resource_provider_stub.h',
        'rounded_corners.h',
        'rounded_corners.cc',
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
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
      ],
    },

    # Support for describing and applying render tree animations.
    {
      'target_name': 'animations',
      'type': 'static_library',
      'sources': [
        'animations/animate_node.cc',
        'animations/animate_node.h',
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
        'animations/animate_node_test.cc',
        'brush_visitor_test.cc',
        'color_rgba_test.cc',
        'node_visitor_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
