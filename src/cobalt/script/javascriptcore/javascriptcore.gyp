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
      'target_name': 'engine',
      'type': 'static_library',
      'sources': [
        'conversion_helpers.cc',
        'conversion_helpers.h',
        'js_object_cache.cc',
        'js_object_cache.h',
        'jsc_call_frame.cc',
        'jsc_call_frame.h',
        'jsc_callback_interface.cc',
        'jsc_debugger.cc',
        'jsc_debugger.h',
        'jsc_engine.cc',
        'jsc_engine.h',
        'jsc_exception_state.cc',
        'jsc_exception_state.h',
        'jsc_gc_markup_visitor.h',
        'jsc_global_object.cc',
        'jsc_global_object.h',
        'jsc_global_object_proxy.cc',
        'jsc_global_object_proxy.h',
        'jsc_object_handle.h',
        'jsc_object_owner.h',
        'jsc_scope.cc',
        'jsc_scope.h',
        'jsc_source_code.cc',
        'jsc_source_code.h',
        'jsc_source_provider.cc',
        'jsc_source_provider.h',
        'script_object_registry.cc',
        'script_object_registry.h',
        'thread_local_hash_table.cc',
        'thread_local_hash_table.h',
        'util/binding_helpers.cc',
        'util/binding_helpers.h',
        'util/exception_helpers.cc',
        'util/exception_helpers.h',
        'wrapper_base.h',
        'wrapper_factory.cc',
        'wrapper_factory.h',
      ],
      'defines': [
        '__DISABLE_WTF_LOGGING__',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore',
        '<(DEPTH)/third_party/WebKit/Source/WTF',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:javascriptcore',
      ],
      'all_dependent_settings': {
        'defines': [
          'ENGINE_DEFINES_ATTRIBUTES_ON_OBJECT',
        ],
      },
      'msvs_disabled_warnings': [
        # dll-interface warnings. Not easily fixed for template types.
        4251,
      ],
    },
    {
      'target_name': 'jsc_engine_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'numeric_conversion_test.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore',
        '<(DEPTH)/third_party/WebKit/Source/WTF',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:javascriptcore',
        'engine',
      ]
    },
    {
      # Standalone executable for JS engine
      'target_name': 'jsc',
      'type': '<(final_executable_type)',
      'sources': [
        'jsc.cc',
      ],
      'defines': [
        '__DISABLE_WTF_LOGGING__',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore',
        '<(DEPTH)/third_party/WebKit/Source/WTF',
      ],
      'dependencies': [
        ':engine',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:standalone_javascript_runner',
        '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:javascriptcore',
      ],
    },
  ],
}
