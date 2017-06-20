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
      'target_name': 'h5vcc',
      'type': 'static_library',
      'sources': [
        'dial/dial_http_request.cc',
        'dial/dial_http_request.h',
        'dial/dial_http_response.cc',
        'dial/dial_http_response.h',
        'dial/dial_server.cc',
        'dial/dial_server.h',
        'h5vcc.cc',
        'h5vcc.h',
        'h5vcc_accessibility.cc',
        'h5vcc_accessibility.h',
        'h5vcc_account_info.cc',
        'h5vcc_account_info.h',
        'h5vcc_audio_config.cc',
        'h5vcc_audio_config.h',
        'h5vcc_audio_config_array.cc',
        'h5vcc_audio_config_array.h',
        'h5vcc_c_val.cc',
        'h5vcc_c_val.h',
        'h5vcc_c_val_key_list.cc',
        'h5vcc_c_val_key_list.h',
        'h5vcc_deep_link_event_target.cc',
        'h5vcc_deep_link_event_target.h',
        'h5vcc_event_listener_container.h',
        'h5vcc_runtime.cc',
        'h5vcc_runtime.h',
        'h5vcc_runtime_event_target.cc',
        'h5vcc_runtime_event_target.h',
        'h5vcc_settings.cc',
        'h5vcc_settings.h',
        'h5vcc_storage.cc',
        'h5vcc_storage.h',
        'h5vcc_system.cc',
        'h5vcc_system.h',
        'h5vcc_trace_event.cc',
        'h5vcc_trace_event.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/build/cobalt_build_id.gyp:cobalt_build_id',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
      ],
      'include_dirs': [
        # For cobalt_build_id.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['enable_account_manager == 1', {
          'sources': [
            'h5vcc_account_manager.cc',
            'h5vcc_account_manager.h',
          ],
          'direct_dependent_settings': {
            'defines': [
              'COBALT_ENABLE_ACCOUNT_MANAGER',
            ],
          },
        }],
        ['enable_crash_log == 1', {
          'sources': [
            'h5vcc_crash_log.cc',
            'h5vcc_crash_log.h',
          ],
          'direct_dependent_settings': {
            'defines': [
              'COBALT_ENABLE_CRASH_LOG',
            ],
          },
        }],
      ],
      'defines': [
        'COBALT_USER_ON_EXIT_STRATEGY="<(cobalt_user_on_exit_strategy)"',
      ],
    },

    {
      'target_name': 'h5vcc_testing',
      'type': 'static_library',
      'sources': [
      ],
    },
  ],
}
