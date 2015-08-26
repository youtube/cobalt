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
    'cobalt_code': 1,
  },
  'targets': [
    {
      # For the convenince of the tests, some test targets depend on
      # dom_parser.gyp:dom_parser. To avoid the dependency cycle between the two
      # gyp files, this target is separated into its own gyp file.
      'target_name': 'dom_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'comment_test.cc',
        'crypto_test.cc',
        'document_test.cc',
        'document_type_test.cc',
        'dom_implementation_test.cc',
        'dom_parser_test.cc',
        'dom_string_map_test.cc',
        'element_test.cc',
        'event_queue_test.cc',
        'event_target_test.cc',
        'event_test.cc',
        'float32_array_test.cc',
        'float64_array_test.cc',
        'html_element_factory_test.cc',
        'html_element_test.cc',
        'local_storage_database_test.cc',
        'location_test.cc',
        'node_dispatch_event_test.cc',
        'node_test.cc',
        'performance_test.cc',
        'rule_matching_test.cc',
        'storage_area_test.cc',
        'text_test.cc',
        'time_ranges_test.cc',
        'typed_array_test.cc',
        'xml_document_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom_testing',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage_test_utils',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'dom_test_deploy',
      'type': 'none',
      'dependencies': [
        'dom_test',
      ],
      'variables': {
        'executable_name': 'dom_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
