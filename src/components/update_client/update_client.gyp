# Copyright 2019 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'update_client',
      'type': 'static_library',
      'sources': [
        'action_runner.cc',
        'action_runner.h',
        'activity_data_service.h',
        'command_line_config_policy.cc',
        'command_line_config_policy.h',
        'component.cc',
        'component.h',
        'component_patcher.cc',
        'component_patcher.h',
        'component_patcher_operation.cc',
        'component_patcher_operation.h',
        'component_unpacker.cc',
        'component_unpacker.h',
        'configurator.h',
        'crx_downloader.cc',
        'crx_downloader.h',
        'crx_update_item.h',
        'network.cc',
        'network.h',
        'patcher.h',
        'persisted_data.cc',
        'persisted_data.h',
        'ping_manager.cc',
        'ping_manager.h',
        'protocol_definition.cc',
        'protocol_definition.h',
        'protocol_handler.cc',
        'protocol_handler.h',
        'protocol_parser.cc',
        'protocol_parser.h',
        'protocol_parser_json.cc',
        'protocol_parser_json.h',
        'protocol_serializer.cc',
        'protocol_serializer.h',
        'protocol_serializer_json.cc',
        'protocol_serializer_json.h',
        'request_sender.cc',
        'request_sender.h',
        'task.h',
        'task_send_uninstall_ping.cc',
        'task_send_uninstall_ping.h',
        'task_traits.h',
        'task_update.cc',
        'task_update.h',
        'unzipper.h',
        'update_checker.cc',
        'update_checker.h',
        'update_client.cc',
        'update_client.h',
        'update_client_errors.h',
        'update_client_internal.h',
        'update_engine.cc',
        'update_engine.h',
        'update_query_params.cc',
        'update_query_params.h',
        'update_query_params_delegate.cc',
        'update_query_params_delegate.h',
        'updater_state.cc',
        'updater_state.h',
        'url_fetcher_downloader.cc',
        'url_fetcher_downloader.h',
        'utils.cc',
        'utils.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/components/crx_file/crx_file.gyp:crx_file',
        '<(DEPTH)/components/client_update_protocol/client_update_protocol.gyp:client_update_protocol',
        '<(DEPTH)/components/prefs/prefs.gyp:prefs',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/url/url.gyp:url',
      ],
      'defines': [
        'LIBXML_READER_ENABLED',
        'LIBXML_WRITER_ENABLED',
      ],
    },
    {
      'target_name': 'update_client_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'utils_unittest.cc',
      ],
      'dependencies': [
        ':update_client',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },
  ]
}
