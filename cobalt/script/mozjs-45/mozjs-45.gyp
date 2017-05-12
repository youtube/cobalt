# Copyright 2016 Google Inc. All Rights Reserved.
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
        'conversion_helpers.cc',
        'mozjs_callback_interface.cc',
        'mozjs_debugger.cc',
        'mozjs_engine.cc',
        'mozjs_exception_state.cc',
        'mozjs_global_environment.cc',
        'mozjs_property_enumerator.cc',
        'mozjs_script_value_factory.cc',
        'mozjs_source_code.cc',
        'promise_wrapper.cc',
        'proxy_handler.cc',
        'referenced_object_map.cc',
        'util/algorithm_helpers.cc',
        'util/exception_helpers.cc',
        'weak_heap_object.cc',
        'weak_heap_object.h',
        'weak_heap_object_manager.cc',
        'weak_heap_object_manager.h',
        'wrapper_factory.cc',
        'wrapper_private.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/third_party/mozjs-45/mozjs-45.gyp:mozjs-45_lib',
        'embed_mozjs_resources_as_header_files',
      ],
      'defines': [ 'ENGINE_SUPPORTS_INT64', ],
      'all_dependent_settings': {
        'defines': [
          # SpiderMonkey bindings implements indexed deleters.
          'ENGINE_SUPPORTS_INDEXED_DELETERS',
          'ENGINE_SUPPORTS_INT64',
          'ENGINE_SUPPORTS_STACK_TRACE_COLUMNS',
          # TODO: Remove this when exact rooting and generational GC is enabled.
          'ENGINE_USES_CONSERVATIVE_ROOTING',
        ],
      },
      'conditions' :[
        ['cobalt_enable_jit == 1', {
          'defines': [ 'ENGINE_SUPPORTS_JIT', ],
        }],
      ],
    },

    {
      # Standalone executable for JS engine
      'target_name': 'mozjs-45',
      'type': '<(final_executable_type)',
      'sources': [
        'mozjs.cc',
      ],
      'dependencies': [
        ':engine',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:standalone_javascript_runner',
        '<(DEPTH)/third_party/mozjs-45/mozjs-45.gyp:mozjs-45_lib',
      ],
    },

    {
      # This target takes specified files and embeds them as header files for
      # inclusion into the binary.
      'target_name': 'embed_mozjs_resources_as_header_files',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'variables': {
        'script_path': '<(DEPTH)/cobalt/build/generate_data_header.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/cobalt/script/mozjs-45/embedded_resources.h',
      },
      'sources': [
        '<(DEPTH)/third_party/promise-polyfill/promise.min.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/byte_length_queuing_strategy.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/count_queuing_strategy.js',
        '<(DEPTH)/cobalt/streams/embedded_scripts/readable_stream.js',
      ],
      'actions': [
        {
          'action_name': 'embed_mozjs_resources_as_header_files',
          'inputs': [
            '<(script_path)',
            '<@(_sources)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': ['python', '<(script_path)', 'MozjsEmbeddedResources', '<(output_path)', '<@(_sources)' ],
          'message': 'Embedding mozjs resources in into header file, "<(output_path)".',
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
