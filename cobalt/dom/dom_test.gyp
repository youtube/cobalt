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
      # For the convenince, some tests depend on dom_parser.gyp:dom_parser. To
      # avoid the dependency cycle between the two gyp files, dom_test is
      # separated into its own gyp file.
      'target_name': 'dom_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'blob_test.cc',
        'comment_test.cc',
        'crypto_test.cc',
        'csp_delegate_test.cc',
        'custom_event_test.cc',
        'data_view_test.cc',
        'document_test.cc',
        'document_type_test.cc',
        'dom_implementation_test.cc',
        'dom_parser_test.cc',
        'dom_rect_list_test.cc',
        'dom_string_map_test.cc',
        'dom_token_list_test.cc',
        'element_test.cc',
        'error_event_test.cc',
        'event_queue_test.cc',
        'event_target_test.cc',
        'event_test.cc',
        'float32_array_test.cc',
        'float64_array_test.cc',
        'font_cache_test.cc',
        'html_element_factory_test.cc',
        'html_element_test.cc',
        'keyboard_event_test.cc',
        'local_storage_database_test.cc',
        'location_test.cc',
        'media_query_list_test.cc',
        'mutation_observer_test.cc',
        'named_node_map_test.cc',
        'node_dispatch_event_test.cc',
        'node_list_live_test.cc',
        'node_list_test.cc',
        'node_test.cc',
        'on_screen_keyboard_test.cc',
        'performance_test.cc',
        'rule_matching_test.cc',
        'screen_test.cc',
        'serializer_test.cc',
        'storage_area_test.cc',
        'text_test.cc',
        'time_ranges_test.cc',
        'typed_array_test.cc',
        'url_utils_test.cc',
        'window_test.cc',
        'xml_document_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom_testing',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
