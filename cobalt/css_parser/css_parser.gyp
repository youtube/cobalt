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

    # Define the platform specific Bison binary.
    'conditions': [
      ['host_os=="win"', {
        'bison_exe': '<(DEPTH)/third_party/bison/bin/bison',
      }, {
        'bison_exe': 'bison',
      }],
    ],
  },

  'targets': [

    # Generates header files from Bison grammar that are privately included
    # by CSS scanner and parser.
    {
      'target_name': 'css_grammar',
      'type': 'none',
      'sources': [
        'grammar.h',
        'grammar.y',
      ],
      # Generated header files are stored in the intermediate directory
      # under their module sub-directory.
      'module_dir': [
        'cobalt/css_parser',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'rules': [
        {
          'rule_name': 'bison',
          'extension': 'y',
          # Don't run through Cygwin on Windows since we want to use a custom
          # Bison executable.
          'msvs_cygwin_shell': 0,
          'outputs': [
            # Tokens and types, included by scanner.
            '<(SHARED_INTERMEDIATE_DIR)/<(_module_dir)/<(RULE_INPUT_ROOT)_generated.h',
            # LALR(1) parser tables and yy*() functions, included by parser.
            '<(SHARED_INTERMEDIATE_DIR)/<(_module_dir)/<(RULE_INPUT_ROOT)_impl_generated.h',
          ],
          'action': [
            '<(bison_exe)',
            '-Wall',
            '-Werror',
            '--defines=<(SHARED_INTERMEDIATE_DIR)/<(_module_dir)/<(RULE_INPUT_ROOT)_generated.h',
            '--output=<(SHARED_INTERMEDIATE_DIR)/<(_module_dir)/<(RULE_INPUT_ROOT)_impl_generated.h',
            '<(RULE_INPUT_PATH)',
          ],
        },
      ],
    },

    {
      'target_name': 'css_parser',
      'type': 'static_library',
      'sources': [
        'animation_shorthand_property_parse_structures.cc',
        'animation_shorthand_property_parse_structures.h',
        'background_shorthand_property_parse_structures.cc',
        'background_shorthand_property_parse_structures.h',
        'border_shorthand_property_parse_structures.cc',
        'border_shorthand_property_parse_structures.h',
        'font_shorthand_property_parse_structures.cc',
        'font_shorthand_property_parse_structures.h',
        'margin_or_padding_shorthand.cc',
        'margin_or_padding_shorthand.h',
        'parser.cc',
        'parser.h',
        'position_parse_structures.cc',
        'position_parse_structures.h',
        'property_declaration.h',
        'ref_counted_util.h',
        'scanner.cc',
        'scanner.h',
        'shadow_property_parse_structures.cc',
        'shadow_property_parse_structures.h',
        'string_pool.h',
        'transition_shorthand_property_parse_structures.cc',
        'transition_shorthand_property_parse_structures.h',
        'trivial_string_piece.h',
        'trivial_type_pairs.h',
      ],
      # Scanner exposes UChar32 in a header.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/icu/source/common',
        ],
      },
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
        '<(DEPTH)/nb/nb.gyp:nb',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'css_grammar',
      ],
    },

    {
      'target_name': 'css_parser_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'parser_test.cc',
        'ref_counted_util_test.cc',
        'scanner_test.cc',
        'trivial_string_piece_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'css_grammar',
        'css_parser',
      ],
    },

    {
      'target_name': 'css_parser_test_deploy',
      'type': 'none',
      'dependencies': [
        'css_parser_test',
      ],
      'variables': {
        'executable_name': 'css_parser_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

  ],
}
