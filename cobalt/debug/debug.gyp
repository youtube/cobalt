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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'debug',
      'type': 'static_library',
      'sources': [
        'component_connector.cc',
        'component_connector.h',
        'console_component.cc',
        'console_component.h',
        'debug_client.cc',
        'debug_client.h',
        'debug_hub.cc',
        'debug_hub.h',
        'debug_script_runner.cc',
        'debug_script_runner.h',
        'debug_server.cc',
        'debug_server.h',
        'debug_server_module.cc',
        'debug_server_module.h',
        'debugger.cc',
        'debugger.h',
        'debugger_event_target.cc',
        'debugger_event_target.h',
        'dom_component.cc',
        'dom_component.h',
        'javascript_debugger_component.cc',
        'javascript_debugger_component.h',
        'json_object.cc',
        'json_object.h',
        'page_component.cc',
        'page_component.h',
        'render_layer.cc',
        'render_layer.h',
        'render_overlay.cc',
        'render_overlay.h',
        'runtime_component.cc',
        'runtime_component.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
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
        }],
        ['cobalt_copy_debug_console==1', {
          'dependencies': [
            'debug_copy_web_files',
          ],
        }],
      ],
    },

    {
      'target_name': 'debug_copy_web_files',
      'type': 'none',
      'variables': {
        'content_web_input_files': [
          '<(DEPTH)/cobalt/debug/content/',
        ],
        'content_web_output_subdir': 'cobalt/debug',
      },
      'includes': [ '<(DEPTH)/cobalt/build/copy_web_data.gypi' ],
    },
  ],
}
