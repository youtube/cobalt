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
  'targets': [
    {
      # Interfaces for interacting with a JavaScript engine and exposing objects
      # to bindings to JavaScript.
      'target_name': 'script',
      'type': 'static_library',
      'sources': [
        'call_frame.h',
        'execution_state.cc',
        'execution_state.h',
        'global_object_proxy.h',
        'scope.h',
        'script_debugger.h',
        'script_runner.cc',
        'script_runner.h',
        'source_code.h',
        'source_provider.h',
        'stack_frame.cc',
        'stack_frame.h',
        'wrappable.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
      ]
    },
    {
      'target_name': 'standalone_javascript_runner',
      'type': 'static_library',
      'sources': [
        'standalone_javascript_runner.cc',
      ],
    },
  ],
}
