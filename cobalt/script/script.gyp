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
        'callback_function.h',
        'callback_interface_traits.h',
        'environment_settings.h',
        'error_report.h',
        'exception_message.cc',
        'exception_message.h',
        'execution_state.cc',
        'execution_state.h',
        'fake_global_environment.h',
        'fake_script_runner.h',
        'global_environment.h',
        'global_object_proxy.h',
        'javascript_engine.h',
        'logging_exception_state.h',
        'opaque_handle.h',
        'promise.h',
        'property_enumerator.h',
        'scope.h',
        'script_debugger.h',
        'script_exception.h',
        'script_runner.cc',
        'script_runner.h',
        'script_value.h',
        'script_value_factory.h',
        'script_value_factory_instantiations.h',
        'sequence.h',
        'source_code.h',
        'source_provider.h',
        'stack_frame.cc',
        'stack_frame.h',
        'union_type.h',
        'union_type_internal.h',
        'util/stack_trace_helpers.h',
        'value_handle.h',
        'wrappable.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/nb/nb.gyp:nb',
      ]
    },
    {
      'target_name': 'standalone_javascript_runner',
      'type': 'static_library',
      'sources': [
        'standalone_javascript_runner.cc',
        'standalone_javascript_runner.h',
      ],
    },
  ],
}
