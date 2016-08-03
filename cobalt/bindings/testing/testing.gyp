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
        'AnonymousIndexedGetterInterface.idl',
        'AnonymousNamedGetterInterface.idl',
        'AnonymousNamedIndexedGetterInterface.idl',
        'ArbitraryInterface.idl',
        'BaseInterface.idl',
        'BooleanTypeTestInterface.idl',
        'CallbackFunctionInterface.idl',
        'CallbackInterfaceInterface.idl',
        'ConditionalInterface.idl',
        'ConstantsInterface.idl',
        'ConstructorInterface.idl',
        'ConstructorWithArgumentsInterface.idl',
        'DerivedGetterSetterInterface.idl',
        'DerivedInterface.idl',
        'DisabledInterface.idl',
        'DOMStringTestInterface.idl',
        'EnumerationInterface.idl',
        'ExceptionObjectInterface.idl',
        'ExceptionsInterface.idl',
        'ExtendedIDLAttributesInterface.idl',
        'GetOpaqueRootInterface.idl',
        'GlobalInterfaceParent.idl',
        'IndexedGetterInterface.idl',
        'InterfaceWithUnsupportedProperties.idl',
        'NamedConstructorInterface.idl',
        'NamedGetterInterface.idl',
        'NamedIndexedGetterInterface.idl',
        'NestedPutForwardsInterface.idl',
        'NoConstructorInterface.idl',
        'NoInterfaceObjectInterface.idl',
        'NullableTypesTestInterface.idl',
        'NumericTypesTestInterface.idl',
        'ObjectTypeBindingsInterface.idl',
        'OperationsTestInterface.idl',
        'PutForwardsInterface.idl',
        'SingleOperationInterface.idl',
        'StringifierAnonymousOperationInterface.idl',
        'StringifierAttributeInterface.idl',
        'StringifierOperationInterface.idl',
        'StaticPropertiesInterface.idl',
        'TargetInterface.idl',
        'UnionTypesInterface.idl',
        'Window.idl',
    ],

    # Partial interfaces and the right-side of "implements"
    # Code will not get generated for these interfaces; they are used to add
    # functionality to other interfaces.
    'dependency_idl_files': [
        'ImplementedInterface.idl',
        'PartialInterface.idl',
        'InterfaceWithUnsupportedProperties_partial.idl',
    ],

    'unsupported_interface_idl_files': [
      'UnsupportedInterface.idl',
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
      'target_name': 'bindings_test_implementation',
      'type': 'static_library',
      'sources': [
        'constants_interface.cc',
        'constructor_interface.cc',
        'exceptions_interface.cc',
        'named_constructor_interface.cc',
        'operations_test_interface.cc',
        'static_properties_interface.cc',
      ],
      'defines': [ '<@(bindings_defines)'],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'bindings_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'boolean_type_bindings_test.cc',
        'callback_function_test.cc',
        'callback_interface_test.cc',
        'conditional_attribute_test.cc',
        'constants_bindings_test.cc',
        'constructor_bindings_test.cc',
        'dependent_interface_test.cc',
        'dom_string_bindings_test.cc',
        'enumeration_bindings_test.cc',
        'exceptions_bindings_test.cc',
        'extended_attributes_test.cc',
        'getter_setter_test.cc',
        'get_opaque_root_test.cc',
        'global_interface_bindings_test.cc',
        'interface_object_test.cc',
        'nullable_types_bindings_test.cc',
        'numeric_type_bindings_test.cc',
        'object_type_bindings_test.cc',
        'operations_bindings_test.cc',
        'optional_arguments_bindings_test.cc',
        'put_forwards_test.cc',
        'static_properties_bindings_test.cc',
        'stringifier_bindings_test.cc',
        'union_type_bindings_test.cc',
        'unsupported_test.cc',
        'variadic_arguments_bindings_test.cc',
      ],
      'defines': [ '<@(bindings_defines)'],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
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
