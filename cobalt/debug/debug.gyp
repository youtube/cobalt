# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
        'backend/command_map.h',
        'backend/console_component.cc',
        'backend/console_component.h',
        'backend/debug_dispatcher.cc',
        'backend/debug_dispatcher.h',
        'backend/debug_module.cc',
        'backend/debug_module.h',
        'backend/debug_script_runner.cc',
        'backend/debug_script_runner.h',
        'backend/dom_component.cc',
        'backend/dom_component.h',
        'backend/javascript_debugger_component.cc',
        'backend/javascript_debugger_component.h',
        'backend/page_component.cc',
        'backend/page_component.h',
        'backend/render_layer.cc',
        'backend/render_layer.h',
        'backend/render_overlay.cc',
        'backend/render_overlay.h',
        'backend/runtime_component.cc',
        'backend/runtime_component.h',
        'backend/script_debugger_component.cc',
        'backend/script_debugger_component.h',
        'command.h',
        'console/debug_hub.cc',
        'console/debug_hub.h',
        'console/debugger.cc',
        'console/debugger.h',
        'console/debugger_event_target.cc',
        'console/debugger_event_target.h',
        'debug_client.cc',
        'debug_client.h',
        'json_object.cc',
        'json_object.h',
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
            'remote/debug_web_server.cc',
            'remote/debug_web_server.h',
          ],
          'dependencies': [
            'devtools/devtools.gyp:devtools',
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
