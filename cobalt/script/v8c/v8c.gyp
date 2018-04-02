# Copyright 2017 Google Inc. All Rights Reserved.
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
      'target_name': 'engine',
      'type': 'static_library',
      'sources': [
        'algorithm_helpers.cc',
        'algorithm_helpers.h',
        'callback_function_conversion.h',
        'conversion_helpers.cc',
        'conversion_helpers.h',
        'entry_scope.h',
        'interface_data.h',
        'isolate_fellowship.cc',
        'isolate_fellowship.h',
        'native_promise.h',
        'scoped_persistent.h',
        'stack_trace_helpers.cc',
        'type_traits.h',
        'union_type_conversion_forward.h',
        'union_type_conversion_impl.h',
        'v8c_array_buffer.cc',
        'v8c_array_buffer.h',
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
        'v8c_script_value_factory.cc',
        'v8c_script_value_factory.h',
        'v8c_source_code.cc',
        'v8c_source_code.h',
        'v8c_typed_arrays.cc',
        'v8c_typed_arrays.h',
        'v8c_user_object_holder.h',
        'v8c_value_handle.cc',
        'v8c_value_handle.h',
        'v8c_wrapper_handle.h',
        'wrapper_factory.cc',
        'wrapper_factory.h',
        'wrapper_private.h',
        '<(DEPTH)/cobalt/script/shared/stub_script_debugger.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/v8/src/v8.gyp:v8',
        '<(DEPTH)/v8/src/v8.gyp:v8_libplatform',
        'embed_v8c_resources_as_header_files',
      ],
      'defines': [
        'ENGINE_SUPPORTS_INT64',
        'ENGINE_SUPPORTS_JIT',
        # The file name to store our V8 startup snapshot file at.  This is a
        # serialized representation of a |v8::Isolate| after completing all
        # tasks prior to creation of the global object (e.g., executing self
        # hosted JavaScript to implement ECMAScript level features).  This
        # state is architecture dependent, and in fact, dependent on anything
        # that could affect JavaScript execution (such as #defines), and thus
        # must be unique with respect to binary, which is why we build it out
        # of platform name and configuration.
        'V8C_INTERNAL_STARTUP_DATA_CACHE_FILE_NAME="<(starboard_platform_name)_<(cobalt_config)_v8_startup_snapshot.bin"',
      ],
      'all_dependent_settings': {
        'defines': [
          'ENGINE_SUPPORTS_INDEXED_DELETERS',
          'ENGINE_SUPPORTS_INT64',
        ],
      },
      # V8 always requires JIT.  |cobalt_enable_jit| must be set to true to
      # use V8.  Failure to do will result in a build failure.
      'conditions' :[
        ['cobalt_enable_jit == 0', {
          'defines': [ '<(gyp_static_assert_false)', ],
        }],
      ],
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
        '<(DEPTH)/v8/src/v8.gyp:v8',
        '<(DEPTH)/v8/src/v8.gyp:v8_libplatform',
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
          'action': ['python', '<(script_path)', 'V8cEmbeddedResources', '<(output_path)', '<@(_sources)' ],
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
