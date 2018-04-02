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
  'includes': [
    '../bindings.gypi',
  ],
  'variables': {
    # Base directory into which generated sources and intermediate files should
    # be generated.
    'bindings_output_dir': '<(SHARED_INTERMEDIATE_DIR)/bindings/testing',

    # Bindings for the interfaces in this list will be generated, and there must
    # be an implementation declared in a header that lives in the same
    # directory of each IDL.
    'source_idl_files': [
        'anonymous_indexed_getter_interface.idl',
        'anonymous_named_getter_interface.idl',
        'anonymous_named_indexed_getter_interface.idl',
        'arbitrary_interface.idl',
        'base_interface.idl',
        'boolean_type_test_interface.idl',
        'callback_function_interface.idl',
        'callback_interface_interface.idl',
        'conditional_interface.idl',
        'constants_interface.idl',
        'constructor_interface.idl',
        'constructor_with_arguments_interface.idl',
        'convert_simple_object_interface.idl',
        'derived_getter_setter_interface.idl',
        'derived_interface.idl',
        'dictionary_interface.idl',
        'disabled_interface.idl',
        'dom_string_test_interface.idl',
        'enumeration_interface.idl',
        'exception_object_interface.idl',
        'exceptions_interface.idl',
        'extended_idl_attributes_interface.idl',
        'garbage_collection_test_interface.idl',
        'global_interface_parent.idl',
        'indexed_getter_interface.idl',
        'interface_with_any.idl',
        'interface_with_any_dictionary.idl',
        'interface_with_date.idl',
        'interface_with_unsupported_properties.idl',
        'named_constructor_interface.idl',
        'named_getter_interface.idl',
        'named_indexed_getter_interface.idl',
        'nested_put_forwards_interface.idl',
        'no_constructor_interface.idl',
        'no_interface_object_interface.idl',
        'nullable_types_test_interface.idl',
        'numeric_types_test_interface.idl',
        'object_type_bindings_interface.idl',
        'operations_test_interface.idl',
        'promise_interface.idl',
        'put_forwards_interface.idl',
        'sequence_user.idl',
        'single_operation_interface.idl',
        'static_properties_interface.idl',
        'stringifier_anonymous_operation_interface.idl',
        'stringifier_attribute_interface.idl',
        'stringifier_operation_interface.idl',
        'target_interface.idl',
        'union_types_interface.idl',
        'window.idl',
    ],

    'generated_header_idl_files': [
        'derived_dictionary.idl',
        'dictionary_with_dictionary_member.idl',
        'test_dictionary.idl',
        'test_enum.idl'
    ],

    # Partial interfaces and the right-side of "implements"
    # Code will not get generated for these interfaces; they are used to add
    # functionality to other interfaces.
    'dependency_idl_files': [
        'implemented_interface.idl',
        'partial_interface.idl',
        'interface_with_unsupported_properties_partial.idl',
    ],

    'unsupported_interface_idl_files': [
      'unsupported_interface.idl',
    ],

    # Specify component for generated window IDL.
    'window_component': 'testing',

    'bindings_dependencies': ['<(DEPTH)/testing/gmock.gyp:gmock'],

    'bindings_defines': [
      'ENABLE_CONDITIONAL_INTERFACE',
      'ENABLE_CONDITIONAL_PROPERTY',
    ],
  },

  'targets': [
    {
      'target_name': 'bindings',
      'type': 'static_library',
      'dependencies': [
        # The generated_bindings target comes from bindings.gypi.
        'generated_bindings',
      ],
    },
    {
      'target_name': 'bindings_test_implementation',
      'type': 'static_library',
      'sources': [
        'constants_interface.cc',
        'constructor_interface.cc',
        'exceptions_interface.cc',
        'garbage_collection_test_interface.cc',
        'named_constructor_interface.cc',
        'operations_test_interface.cc',
        'put_forwards_interface.cc',
        'static_properties_interface.cc',
      ],
      'defines': [ '<@(bindings_defines)'],
      'dependencies': [
        'generated_types',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'generated_types',
      ],
    },
    {
      'target_name': 'bindings_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'array_buffers_test.cc',
        'any_bindings_test.cc',
        'any_dictionary_bindings_test.cc',
        'boolean_type_bindings_test.cc',
        'callback_function_test.cc',
        'callback_interface_test.cc',
        'conditional_attribute_test.cc',
        'constants_bindings_test.cc',
        'constructor_bindings_test.cc',
        'convert_simple_object_test.cc',
        'date_bindings_test.cc',
        'dependent_interface_test.cc',
        'dictionary_test.cc',
        'dom_string_bindings_test.cc',
        'enumeration_bindings_test.cc',
        'evaluate_script_test.cc',
        'exceptions_bindings_test.cc',
        'extended_attributes_test.cc',
        'garbage_collection_test.cc',
        'get_own_property_descriptor.cc',
        'getter_setter_test.cc',
        'global_interface_bindings_test.cc',
        'interface_object_test.cc',
        'nullable_types_bindings_test.cc',
        'numeric_type_bindings_test.cc',
        'object_type_bindings_test.cc',
        'operations_bindings_test.cc',
        'optional_arguments_bindings_test.cc',
        'promise_test.cc',
        'put_forwards_test.cc',
        'sequence_bindings_test.cc',
        'stack_trace_test.cc',
        'static_properties_bindings_test.cc',
        'stringifier_bindings_test.cc',
        'union_type_bindings_test.cc',
        'unsupported_test.cc',
        'variadic_arguments_bindings_test.cc',
      ],
      'defines': [ '<@(bindings_defines)'],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'bindings',
        'bindings_test_implementation',
      ],
    },

    {
      'target_name': 'bindings_test_deploy',
      'type': 'none',
      'dependencies': [
        'bindings_test',
      ],
      'variables': {
        'executable_name': 'bindings_test',
      },
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'bindings_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'bindings_sandbox_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:standalone_javascript_runner',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        'bindings',
        'bindings_test_implementation',
      ]
    },

    {
      'target_name': 'bindings_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'bindings_sandbox',
      ],
      'variables': {
        'executable_name': 'bindings_sandbox',
      },
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },
  ],
}
