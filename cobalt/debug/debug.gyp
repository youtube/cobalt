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
  'variables': {
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'debug',
      'type': 'static_library',
      'sources': [
        'debug_hub.cc',
        'debug_hub.h',
        'debug_server.cc',
        'debug_server.h',
        'debugger.cc',
        'debugger.h',
        'debugger_event_target.cc',
        'debugger_event_target.h',
        'json_object.cc',
        'json_object.h',
        'system_stats_tracker.cc',
        'system_stats_tracker.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/net/net.gyp:http_server',
      ],
      'conditions': [
        ['enable_remote_debugging==1', {
          'sources': [
            'debug_web_server.cc',
            'debug_web_server.h',
          ],
          'defines': [ 'ENABLE_REMOTE_DEBUGGING', ],
          'all_dependent_settings': {
            'defines': [ 'ENABLE_REMOTE_DEBUGGING', ],
          },
          'dependencies': [
            'debug_copy_web_files',
          ],
        }],
      ],
    },

    {
      'target_name': 'debug_copy_web_files',
      'type': 'none',
      'actions': [
        {
          'action_name': 'debug_copy_web_files',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/debug/content/',
            ],
            'output_dir': 'cobalt/debug',
          },
          'includes': [ '../build/copy_web_data.gypi' ],
        },
      ],
    },
  ],
}
