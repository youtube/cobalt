# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
  'target_defaults': {
    'conditions': [
        ['(target_arch=="arm64" or target_arch=="x64") and disable_v8_pointer_compression==0', {
          'defines': [
            # enables pointer compression on 64 bit platforms for Cobalt.
            'V8_COMPRESS_POINTERS',
            'V8_31BIT_SMIS_ON_64BIT_ARCH',
          ],
          'all_dependent_settings': {
            'defines': [
                'V8_COMPRESS_POINTERS',
                'V8_31BIT_SMIS_ON_64BIT_ARCH',
            ],
          },
        }],
    ],
  },
  'targets': [
    {
      'target_name': 'engine',
      'type': 'static_library',
      'sources': [
        'algorithm_helpers.cc',
        'algorithm_helpers.h',
        'callback_function_conversion.h',
        'cobalt_platform.cc',
        'cobalt_platform.h',
        'common_v8c_bindings_code.cc',
        'common_v8c_bindings_code.h',
        'conversion_helpers.cc',
        'conversion_helpers.h',
        'entry_scope.h',
        'interface_data.h',
        'isolate_fellowship.cc',
        'isolate_fellowship.h',
        'native_promise.h',
        'scoped_persistent.h',
        'stack_trace_helpers.cc',
        'switches.cc',
        'switches.h',
        'type_traits.h',
        'union_type_conversion_forward.h',
        'union_type_conversion_impl.h',
        'v8c_array_buffer.cc',
        'v8c_array_buffer.h',
        'v8c_array_buffer_view.cc',
        'v8c_array_buffer_view.h',
        'v8c_callback_function.h',
        'v8c_callback_interface.cc',
        'v8c_callback_interface.h',
        'v8c_callback_interface_holder.h',
        'v8c_data_view.cc',
        'v8c_data_view.h',
        'v8c_engine.cc',
        'v8c_engine.h',
        'v8c_exception_state.cc',
        'v8c_exception_state.h',
        'v8c_global_environment.cc',
        'v8c_global_environment.h',
        'v8c_heap_tracer.cc',
        'v8c_heap_tracer.h',
        'v8c_property_enumerator.h',
        'v8c_script_debugger.cc',
        'v8c_script_debugger.h',
        'v8c_script_value_factory.cc',
        'v8c_script_value_factory.h',
        'v8c_source_code.cc',
        'v8c_source_code.h',
        'v8c_tracing_controller.cc',
        'v8c_tracing_controller.h',
        'v8c_typed_arrays.cc',
        'v8c_typed_arrays.h',
        'v8c_user_object_holder.h',
        'v8c_value_handle.cc',
        'v8c_value_handle.h',
        'v8c_wrapper_handle.h',
        'wrapper_factory.cc',
        'wrapper_factory.h',
        'wrapper_private.cc',
        'wrapper_private.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/configuration/configuration.gyp:configuration',
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/third_party/v8/v8.gyp:v8',
        '<(DEPTH)/third_party/v8/v8.gyp:v8_libplatform',
        '<(DEPTH)/third_party/v8/v8.gyp:v8_base_without_compiler',
        'embed_v8c_resources_as_header_files',
      ],
      'defines': [
        'ENGINE_SUPPORTS_INT64',
      ],
      'all_dependent_settings': {
        'defines': [
          'ENGINE_SUPPORTS_INDEXED_DELETERS',
          'ENGINE_SUPPORTS_INT64',
        ],
      },
    },

    {
      # Standalone shell executable for V8 within Cobalt.
      'target_name': 'v8c',
      'type': '<(final_executable_type)',
      'sources': [
        'v8c.cc',
      ],
      'dependencies': [
        ':engine',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:standalone_javascript_runner',
        '<(DEPTH)/third_party/v8/v8.gyp:v8',
        '<(DEPTH)/third_party/v8/v8.gyp:v8_base_without_compiler',
        '<(DEPTH)/third_party/v8/v8.gyp:v8_libplatform',
      ],
    },

    {
      # This target takes specified files and embeds them as header files for
      # inclusion into the binary.
      'target_name': 'embed_v8c_resources_as_header_files',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'variables': {
        'script_path': '<(DEPTH)/cobalt/build/generate_data_header.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/cobalt/script/v8c/embedded_resources.h',
      },
      'sources': [
        '<(DEPTH)/cobalt/fetch/embedded_scripts/fetch.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/byte_length_queuing_strategy.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/count_queuing_strategy.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/readable_stream.js',
      ],
      'actions': [
        {
          'action_name': 'embed_v8c_resources_as_header_files',
          'inputs': [
            '<(script_path)',
            '<@(_sources)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': ['python2', '<(script_path)', 'V8cEmbeddedResources', '<(output_path)', '<@(_sources)' ],
          'message': 'Embedding v8c resources in into header file, "<(output_path)".',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ],
}
