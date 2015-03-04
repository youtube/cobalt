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
      'target_name': 'cssom',
      'type': 'static_library',
      'sources': [
        'css_parser.h',
        'css_rule.h',
        'css_rule_list.cc',
        'css_rule_list.h',
        'css_style_declaration.cc',
        'css_style_declaration.h',
        'css_style_rule.cc',
        'css_style_rule.h',
        'css_style_sheet.cc',
        'css_style_sheet.h',
        'inherited_value.cc',
        'inherited_value.h',
        'initial_value.cc',
        'initial_value.h',
        'length_value.cc',
        'length_value.h',
        'none_value.cc',
        'none_value.h',
        'number_value.cc',
        'number_value.h',
        'property_value.h',
        'property_value_visitor.h',
        'rgba_color_value.cc',
        'rgba_color_value.h',
        'scale_function.cc',
        'scale_function.h',
        'selector.h',
        'selector_visitor.h',
        'string_value.cc',
        'string_value.h',
        'style_sheet.h',
        'style_sheet_list.cc',
        'style_sheet_list.h',
        'transform_function.h',
        'transform_function_visitor.h',
        'transform_list_value.cc',
        'transform_list_value.h',
        'translate_function.cc',
        'translate_function.h',
        'type_selector.cc',
        'type_selector.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
      ],
    },

    {
      'target_name': 'cssom_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'css_style_declaration_test.cc',
        'css_style_sheet_test.cc',
        'property_value_visitor_test.cc',
        'selector_visitor_test.cc',
        'style_sheet_list_test.cc',
        'transform_function_visitor_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'cssom',
      ],
    },

    {
      'target_name': 'cssom_test_deploy',
      'type': 'none',
      'dependencies': [
        'cssom_test',
      ],
      'variables': {
        'executable_name': 'cssom_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
