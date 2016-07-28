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
  'includes': [ '../build/contents_dir.gypi' ],

  'targets': [
    {
      'target_name': 'webdriver',
      'type': 'static_library',
      'conditions': [
        ['enable_webdriver==1', {
          'sources': [
            'algorithms.cc',
            'algorithms.h',
            'dispatcher.cc',
            'dispatcher.h',
            'element_driver.cc',
            'element_driver.h',
            'keyboard.cc',
            'keyboard.h',
            'protocol/capabilities.cc',
            'protocol/capabilities.h',
            'protocol/cookie.h',
            'protocol/cookie.cc',
            'protocol/element_id.cc',
            'protocol/element_id.h',
            'protocol/frame_id.cc',
            'protocol/frame_id.h',
            'protocol/keys.cc',
            'protocol/keys.h',
            'protocol/proxy.cc',
            'protocol/proxy.h',
            'protocol/log_entry.cc',
            'protocol/log_entry.h',
            'protocol/log_type.cc',
            'protocol/log_type.h',
            'protocol/response.cc',
            'protocol/response.h',
            'protocol/search_strategy.cc',
            'protocol/search_strategy.h',
            'protocol/server_status.cc',
            'protocol/server_status.h',
            'protocol/session_id.h',
            'protocol/script.cc',
            'protocol/script.h',
            'protocol/size.cc',
            'protocol/size.h',
            'protocol/window_id.cc',
            'protocol/window_id.h',
            'script_executor.cc',
            'script_executor.h',
            'search.cc',
            'search.h',
            'server.cc',
            'server.h',
            'session_driver.cc',
            'session_driver.h',
            'web_driver_module.cc',
            'web_driver_module.h',
            'window_driver.cc',
            'window_driver.h',
          ],
          'defines': [ 'ENABLE_WEBDRIVER', ],
          'all_dependent_settings': {
            'defines': [ 'ENABLE_WEBDRIVER', ],
          },
        }],
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/net/net.gyp:http_server',
        'copy_webdriver_data',
      ],
    },

    {
      'target_name': 'webdriver_test',
      'type': '<(gtest_target_type)',
      'conditions': [
        ['enable_webdriver==1', {
          'sources': [
            'get_element_text_test.cc',
            'is_displayed_test.cc',
            'keyboard_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'webdriver',
      ],
      'actions': [
        {
          'action_name': 'webdriver_test_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/webdriver/testdata/',
            ],
            'output_dir': 'cobalt/webdriver_test',
          },
          'includes': ['../build/copy_test_data.gypi'],
        }
      ],
    },

    {
      'target_name': 'webdriver_test_deploy',
      'type': 'none',
      'dependencies': [
        'webdriver_test',
      ],
      'variables': {
        'executable_name': 'webdriver_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

    {
      'target_name': 'copy_webdriver_data',
      'type': 'none',
      'copies': [
        {
          'destination': '<(static_contents_output_data_dir)/webdriver',
          'conditions': [
            ['enable_webdriver==1', {
              'files': ['content/webdriver-init.js'],
            }, {
              'files': [],
            }],
          ],
        },
      ],
    },
  ],
}
