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
    {
      'target_name': 'layout',
      'type': 'static_library',
      'sources': [
        'anonymous_block_box.cc',
        'anonymous_block_box.h',
        'base_direction.h',
        'benchmark_stat_names.cc',
        'benchmark_stat_names.h',
        'block_container_box.cc',
        'block_container_box.h',
        'block_formatting_block_container_box.cc',
        'block_formatting_block_container_box.h',
        'block_formatting_context.cc',
        'block_formatting_context.h',
        'block_level_replaced_box.cc',
        'block_level_replaced_box.h',
        'box.cc',
        'box.h',
        'box_generator.cc',
        'box_generator.h',
        'container_box.cc',
        'container_box.h',
        'letterboxed_image.cc',
        'letterboxed_image.h',
        'formatting_context.h',
        'initial_containing_block.cc',
        'initial_containing_block.h',
        'inline_container_box.cc',
        'inline_container_box.h',
        'inline_formatting_context.cc',
        'inline_formatting_context.h',
        'inline_level_replaced_box.cc',
        'inline_level_replaced_box.h',
        'insets_layout_unit.cc',
        'insets_layout_unit.h',
        'layout.cc',
        'layout.h',
        'layout_unit.h',
        'layout_boxes.cc',
        'layout_boxes.h',
        'layout_manager.cc',
        'layout_manager.h',
        'layout_stat_tracker.cc',
        'layout_stat_tracker.h',
        'line_box.cc',
        'line_box.h',
        'line_wrapping.cc',
        'line_wrapping.h',
        'paragraph.cc',
        'paragraph.h',
        'point_layout_unit.cc',
        'point_layout_unit.h',
        'replaced_box.cc',
        'replaced_box.h',
        'rect_layout_unit.cc',
        'rect_layout_unit.h',
        'render_tree_animations.h',
        'size_layout_unit.cc',
        'size_layout_unit.h',
        'text_box.cc',
        'text_box.h',
        'topmost_event_target.cc',
        'topmost_event_targer.h',
        'used_style.cc',
        'used_style.h',
        'vector2d_layout_unit.cc',
        'vector2d_layout_unit.h',
        'white_space_processing.cc',
        'white_space_processing.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:animations',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      # Exporting dom so that layout_test gets the transitive include paths to
      # include generated headers.
      'export_dependent_settings': [
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
      ],
      'conditions': [
        ['cobalt_enable_lib == 1', {
          'defines' : ['FORCE_VIDEO_EXTERNAL_MESH'],
        }],
      ],
    },

    {
      'target_name': 'layout_testing',
      'type': 'static_library',
      'sources': [
        'testing/mock_image.h',
      ],
    },

    {
      'target_name': 'layout_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'letterboxed_image_test.cc',
        'layout_unit_test.cc',
        'used_style_test.cc',
        'white_space_processing_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/layout/layout.gyp:layout_testing',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'layout',
      ],
    },

    {
      'target_name': 'layout_test_deploy',
      'type': 'none',
      'dependencies': [
        'layout_test',
      ],
      'variables': {
        'executable_name': 'layout_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
