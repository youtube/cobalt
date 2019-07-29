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
        'backend/console_agent.cc',
        'backend/console_agent.h',
        'backend/css_agent.cc',
        'backend/css_agent.h',
        'backend/debug_backend.cc',
        'backend/debug_backend.h',
        'backend/debug_dispatcher.cc',
        'backend/debug_dispatcher.h',
        'backend/debug_module.cc',
        'backend/debug_module.h',
        'backend/debug_script_runner.cc',
        'backend/debug_script_runner.h',
        'backend/dom_agent.cc',
        'backend/dom_agent.h',
        'backend/log_agent.cc',
        'backend/log_agent.h',
        'backend/page_agent.cc',
        'backend/page_agent.h',
        'backend/render_layer.cc',
        'backend/render_layer.h',
        'backend/render_overlay.cc',
        'backend/render_overlay.h',
        'backend/runtime_agent.cc',
        'backend/runtime_agent.h',
        'backend/script_debugger_agent.cc',
        'backend/script_debugger_agent.h',
        'backend/tracing_agent.cc',
        'backend/tracing_agent.h',
        'command.h',
        'console/debug_hub.cc',
        'console/debug_hub.h',
        'console/debugger_event_target.cc',
        'console/debugger_event_target.h',
        'debug_client.cc',
        'debug_client.h',
        'json_object.cc',
        'json_object.h',
        'remote/debug_web_server.cc',
        'remote/debug_web_server.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/net/net.gyp:http_server',
        'console_command_manager',
        'copy_backend_web_files',
        'copy_console_web_files',
        'copy_remote_web_files',
        'remote/devtools/devtools.gyp:devtools',
      ],
    },

    {
      # Anything that implements a console command can depend on this without
      # depending on the whole debug module.
      'target_name': 'console_command_manager',
      'type': 'static_library',
      'conditions': [
        # Everything is conditional so implementers of commands can depend on
        # this unconditionally.
        ['enable_debugger == 1', {
          'sources': [
            'console/command_manager.cc',
            'console/command_manager.h',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
          ],
        }],
      ],
    },

    {
      'target_name': 'copy_backend_web_files',
      'type': 'none',
      'variables': {
        'content_web_input_files': [
          '<(DEPTH)/cobalt/debug/backend/content/',
        ],
        'content_web_output_subdir': 'cobalt/debug/backend',
      },
      'includes': [ '<(DEPTH)/cobalt/build/copy_web_data.gypi' ],
    },

    {
      'target_name': 'copy_console_web_files',
      'type': 'none',
      'variables': {
        'content_web_input_files': [
          '<(DEPTH)/cobalt/debug/console/content/',
        ],
        'content_web_output_subdir': 'cobalt/debug/console',
      },
      'includes': [ '<(DEPTH)/cobalt/build/copy_web_data.gypi' ],
    },

    {
      'target_name': 'copy_remote_web_files',
      'type': 'none',
      'variables': {
        'content_web_input_files': [
          '<(DEPTH)/cobalt/debug/remote/content/',
        ],
        'content_web_output_subdir': 'cobalt/debug/remote',
      },
      'includes': [ '<(DEPTH)/cobalt/build/copy_web_data.gypi' ],
    },
  ],
}
