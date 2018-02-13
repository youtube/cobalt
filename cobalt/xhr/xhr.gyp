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
      'target_name': 'xhr',
      'type': 'static_library',
      'sources': [
        'xhr_response_data.cc',
        'xhr_response_data.h',
        'xml_http_request.cc',
        'xml_http_request.h',
        'xml_http_request_event_target.cc',
        'xml_http_request_event_target.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
      ],
      'conditions': [
        ['enable_xhr_header_filtering == 1', {
          'sources': [
            'xhr_modify_headers.h',
          ],
          'dependencies': [
            '<@(cobalt_platform_dependencies)',
          ],
          'defines': [
            'COBALT_ENABLE_XHR_HEADER_FILTERING',
          ],
          'direct_dependent_settings': {
            'defines': [
              'COBALT_ENABLE_XHR_HEADER_FILTERING',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'xhr_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'xhr_response_data_test.cc',
        'xml_http_request_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'xhr',

        # TODO: Remove the dependency below, it works around the fact that
        #       ScriptValueFactory has non-virtual method CreatePromise().
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
      ],
    },
    {
      'target_name': 'xhr_test_deploy',
      'type': 'none',
      'dependencies': [
        'xhr_test',
      ],
      'variables': {
        'executable_name': 'xhr_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
