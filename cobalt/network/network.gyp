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

  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'network',
      'type': 'static_library',
      'sources': [
        'socket_address_parser.cc',
        'socket_address_parser.h',
        'cookie_jar_impl.cc',
        'cookie_jar_impl.h',
        'net_poster.cc',
        'net_poster.h',
        'local_network.cc',
        'local_network.h',
        'network_delegate.cc',
        'network_delegate.h',
        'network_event.h',
        'network_module.cc',
        'network_module.h',
        'network_system.h',
        'persistent_cookie_store.cc',
        'persistent_cookie_store.h',
        'proxy_config_service.h',
        'starboard/network_system.cc',
        'starboard/proxy_config_service.cc',
        'starboard/user_agent_string_factory_starboard.cc',
        'switches.cc',
        'switches.h',
        'url_request_context.cc',
        'url_request_context.h',
        'url_request_context_getter.cc',
        'url_request_context_getter.h',
        'user_agent_string_factory.cc',
        'user_agent_string_factory.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/network_bridge/network_bridge.gyp:network_bridge',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/cobalt/build/cobalt_build_id.gyp:cobalt_build_id',
      ],
      'conditions': [
        ['in_app_dial == 1', {
          'dependencies': [
            # DialService depends on http server.
            '<(DEPTH)/net/net.gyp:http_server',
          ],
        }],
        ['enable_network_logging == 1', {
          'sources': [
            'cobalt_net_log.cc',
            'cobalt_net_log.h',
            'net_log_constants.cc',
            'net_log_constants.h',
            'net_log_logger.cc',
            'net_log_logger.h',
          ],
          'defines': [
            'ENABLE_NETWORK_LOGGING',
          ],
        }],
        ['cobalt_enable_lib == 1', {
          'defines' : ['COBALT_ENABLE_LIB'],
        }],
      ],
      'export_dependent_settings': [
        '<(DEPTH)/net/net.gyp:net',
      ],
      'include_dirs': [
        # For cobalt_build_id.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'copies': [
      {
        'destination': '<(sb_static_contents_output_data_dir)/ssl',
        'files': ['<(static_contents_source_dir)/ssl/certs/'],
      }],
      'all_dependent_settings': {
        'variables': {
          'content_deploy_subdirs': [ 'ssl/certs' ]
        }
      },
    },
    {
      'target_name': 'network_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'local_network_test.cc',
        'persistent_cookie_store_test.cc',
        'user_agent_string_factory_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'network',
      ],
    },
    {
      'target_name': 'network_test_deploy',
      'type': 'none',
      'dependencies': [
        'network_test',
      ],
      'variables': {
        'executable_name': 'network_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
