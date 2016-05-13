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
        'h5vcc_account_info.cc',
        'h5vcc_account_info.h',
        'h5vcc_audio_config.cc',
        'h5vcc_audio_config.h',
        'h5vcc_audio_config_array.cc',
        'h5vcc_audio_config_array.h',
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
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/build/cobalt_build_id.gyp:cobalt_build_id',
      ],
      'include_dirs': [
        # For cobalt_build_id.h
        '<(SHARED_INTERMEDIATE_DIR)',
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
