# Copyright 2015 Google Inc. All Rights Reserved.
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
      'target_name': 'cssom_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'cascade_precedence_test.cc',
        'cascaded_style_test.cc',
        'computed_style_test.cc',
        'css_computed_style_data_property_set_matcher_test.cc',
        'css_computed_style_data_test.cc',
        'css_computed_style_declaration_test.cc',
        'css_declared_style_data_test.cc',
        'css_declared_style_declaration_test.cc',
        'css_font_face_declaration_data_test.cc',
        'css_font_face_rule_test.cc',
        'css_grouping_rule_test.cc',
        'css_property_definitions_test.cc',
        'css_rule_list_test.cc',
        'css_rule_visitor_test.cc',
        'css_style_sheet_test.cc',
        'css_transition_set_test.cc',
        'interpolate_property_value_test.cc',
        'keyword_value_test.cc',
        'media_feature_test.cc',
        'media_list_test.cc',
        'media_query_test.cc',
        'property_value_is_equal_test.cc',
        'property_value_to_string_test.cc',
        'property_value_visitor_test.cc',
        'selector_test.cc',
        'selector_tree_test.cc',
        'selector_visitor_test.cc',
        'specificity_test.cc',
        'style_sheet_list_test.cc',
        'timing_function_test.cc',
        'transform_function_visitor_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
