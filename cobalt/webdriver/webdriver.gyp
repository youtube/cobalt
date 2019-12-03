# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

  'variables': {
    'optimize_target_for_speed': 1,
  },

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
            'protocol/button.cc',
            'protocol/button.h',
            'protocol/capabilities.cc',
            'protocol/capabilities.h',
            'protocol/cookie.cc',
            'protocol/cookie.h',
            'protocol/element_id.cc',
            'protocol/element_id.h',
            'protocol/frame_id.cc',
            'protocol/frame_id.h',
            'protocol/keys.cc',
            'protocol/keys.h',
            'protocol/location.cc',
            'protocol/location.h',
            'protocol/log_entry.cc',
            'protocol/log_entry.h',
            'protocol/log_type.cc',
            'protocol/log_type.h',
            'protocol/moveto.cc',
            'protocol/moveto.h',
            'protocol/proxy.cc',
            'protocol/proxy.h',
            'protocol/rect.cc',
            'protocol/rect.h',
            'protocol/response.cc',
            'protocol/response.h',
            'protocol/script.cc',
            'protocol/script.h',
            'protocol/search_strategy.cc',
            'protocol/search_strategy.h',
            'protocol/server_status.cc',
            'protocol/server_status.h',
            'protocol/session_id.h',
            'protocol/size.cc',
            'protocol/size.h',
            'protocol/window_id.cc',
            'protocol/window_id.h',
            'screenshot.cc',
            'screenshot.h',
            'script_executor.cc',
            'script_executor.h',
            'script_executor_params.cc',
            'script_executor_params.h',
            'script_executor_result.h',
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
            'screencast/screencast_module.cc',
            'screencast/screencast_module.h',
          ],
          'dependencies': [ 'copy_webdriver_data', ],
          'defines': [
            'ENABLE_WEBDRIVER',
          ],
          'all_dependent_settings': {
            'defines': [ 'ENABLE_WEBDRIVER', ],
          },
        }],
        ['enable_webdriver==0', {
          'sources': [
          'stub_web_driver_module.cc'
          ]
        }],
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:http_server',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
    },
    {
      'target_name': 'copy_webdriver_data',
      'type': 'none',
      'copies': [
        {
          'destination': '<(sb_static_contents_output_data_dir)/webdriver',
          'files': ['content/webdriver-init.js'],
        },
      ],
      'all_dependent_settings': {
        'variables': {
          'content_deploy_subdirs': [ 'webdriver' ]
        }
      },
    },
  ],
}
