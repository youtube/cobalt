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
    {
      'target_name': 'layout',
      'type': 'static_library',
      'sources': [
        'anonymous_block_box.cc',
        'anonymous_block_box.h',
        'block_container_box.cc',
        'block_container_box.h',
        'block_formatting_block_container_box.cc',
        'block_formatting_block_container_box.h',
        'block_formatting_context.cc',
        'block_formatting_context.h',
        'box.cc',
        'box.h',
        'box_generator.cc',
        'box_generator.h',
        'cascaded_style.cc',
        'cascaded_style.h',
        'computed_style.cc',
        'computed_style.h',
        'container_box.cc',
        'container_box.h',
        'declared_style.cc',
        'declared_style.h',
        'formatting_context.h',
        'html_elements.cc',
        'html_elements.h',
        'inline_container_box.cc',
        'inline_container_box.h',
        'inline_formatting_context.cc',
        'inline_formatting_context.h',
        'layout.cc',
        'layout.h',
        'layout_manager.cc',
        'layout_manager.h',
        'line_box.cc',
        'line_box.h',
        'replaced_box.cc',
        'replaced_box.h',
        'specified_style.cc',
        'specified_style.h',
        'text_box.cc',
        'text_box.h',
        'transition_render_tree_animations.h',
        'used_style.cc',
        'used_style.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:animations',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'embed_resources_as_header_files',
      ],
      'conditions': [
        ['actual_target_arch=="win"', {
          # Disable warning C4702: unreachable code in xtree.
          'msvs_disabled_warnings': [ 4702 ],
        }],
      ],
    },

    {
      # This target takes all files in the embedded_resources directory (e.g.
      # the user agent style sheet) and embeds them as header files for
      # inclusion into the binary.
      'target_name': 'embed_resources_as_header_files',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'variables': {
        'script_path': '<(DEPTH)/lbshell/build/generate_data_header.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/cobalt/layout/embedded_resources.h',
        'input_directory': 'embedded_resources',
      },
      'sources': [
        '<(input_directory)/user_agent_style_sheet.css',
      ],
      'actions': [
        {
          'action_name': 'embed_resources_as_header_files',
          'inputs': [
            '<(script_path)',
            '<@(_sources)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': ['python', '<(script_path)', 'LayoutEmbeddedResources', '<(input_directory)', '<(output_path)'],
          'message': 'Embedding layout resources in "<(input_directory)" into header file, "<(output_path)".',
          'msvs_cygwin_shell': 1,
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },

    {
      'target_name': 'layout_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'cascaded_style_test.cc',
        'computed_style_test.cc',
        'declared_style_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'layout',
      ],
      'conditions': [
        ['actual_target_arch=="win"', {
          # Disable warning C4702: unreachable code in xtree.
          'msvs_disabled_warnings': [ 4702 ],
        }],
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
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
