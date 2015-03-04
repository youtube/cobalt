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
  'targets': [
    {
      'target_name': 'layout',
      'type': 'static_library',
      'sources': [
        'box.cc',
        'box.h',
        'box_generator.cc',
        'box_generator.h',
        'cascaded_style.cc',
        'cascaded_style.h',
        'computed_style.cc',
        'computed_style.h',
        'containing_block.cc',
        'containing_block.h',
        'declared_style.cc',
        'declared_style.h',
        'html_elements.cc',
        'html_elements.h',
        'initial_style.cc',
        'initial_style.h',
        'layout.cc',
        'layout.h',
        'layout_manager.cc',
        'layout_manager.h',
        'specified_style.cc',
        'specified_style.h',
        'text_box.cc',
        'text_box.h',
        'used_style.cc',
        'used_style.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
      ],
    },

    {
      'target_name': 'layout_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'layout_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
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
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
