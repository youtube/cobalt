# Copyright 2018 Google Inc. All Rights Reserved.
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
      'target_name': 'dom_parser_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'html_decoder_test.cc',
        'xml_decoder_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom/testing/dom_testing.gyp:dom_testing',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'dom_parser_test_deploy',
      'type': 'none',
      'dependencies': [
        'dom_parser_test',
      ],
      'variables': {
        'executable_name': 'dom_parser_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
