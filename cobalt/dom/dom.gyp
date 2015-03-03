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
      'target_name': 'dom',
      'type': 'static_library',
      'sources': [
        'attr.cc',
        'attr.h',
        'comment.cc',
        'comment.h',
        'document.cc',
        'document.h',
        'document_builder.cc',
        'document_builder.h',
        'dom_token_list.cc',
        'dom_token_list.h',
        'element.cc',
        'element.h',
        'event.cc',
        'event.h',
        'event_listener.h',
        'event_queue.cc',
        'event_queue.h',
        'event_target.cc',
        'event_target.h',
        'html_body_element.cc',
        'html_body_element.h',
        'html_collection.cc',
        'html_collection.h',
        'html_div_element.cc',
        'html_div_element.h',
        'html_element.cc',
        'html_element.h',
        'html_element_factory.cc',
        'html_element_factory.h',
        'html_head_element.cc',
        'html_head_element.h',
        'html_html_element.cc',
        'html_html_element.h',
        'html_link_element.cc',
        'html_link_element.h',
        'html_parser.cc',
        'html_parser.h',
        'html_script_element.cc',
        'html_script_element.h',
        'html_serializer.cc',
        'html_serializer.h',
        'html_span_element.cc',
        'html_span_element.h',
        'html_unknown_element.cc',
        'html_unknown_element.h',
        'location.cc',
        'location.h',
        'media_error.h',
        'keyboard_event.cc',
        'keyboard_event.h',
        'named_node_map.cc',
        'named_node_map.h',
        'navigator.cc',
        'navigator.h',
        'node.cc',
        'node.h',
        'node_children_iterator.h',
        'node_descendants_iterator.h',
        'stats.cc',
        'stats.h',
        'text.cc',
        'text.h',
        'time_ranges.cc',
        'time_ranges.h',
        'window.cc',
        'window.h',
        'ui_event.cc',
        'ui_event.h',
        'ui_event_with_key_state.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/loader/loader.gyp:resource_loader',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
      ],
    },

    {
      'target_name': 'dom_copy_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'dom_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/dom/testdata',
            ],
            'output_dir': 'cobalt/browser',
          },
          'includes': [ '../build/copy_data.gypi' ],
        },
      ],
    },

    {
      'target_name': 'dom_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'comment_test.cc',
        'document_builder_test.cc',
        'document_test.cc',
        'element_test.cc',
        'event_queue_test.cc',
        'event_target_test.cc',
        'html_parser_test.cc',
        'location_test.cc',
        'node_test.cc',
        'testing/fake_node.h',
        'testing/gtest_workarounds.h',
        'testing/html_collection_testing.h',
        'testing/mock_event_listener.h',
        'testing/parent_node_testing.cc',
        'testing/parent_node_testing.h',
        'testing/switches.cc',
        'testing/switches.h',
        'text_test.cc',
      ],
      'dependencies': [
        'dom',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/loader/loader.gyp:fake_resource_loader',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'dom_copy_test_data',
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
