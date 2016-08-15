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
        'mozjs_global_object_proxy.cc',
        'mozjs_property_enumerator.cc',
        'mozjs_source_code.cc',
        'proxy_handler.cc',
        'util/exception_helpers.cc',
        'wrapper_factory.cc',
        'wrapper_private.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/script/script.gyp:script',
        '<(DEPTH)/third_party/mozjs/mozjs.gyp:mozjs_lib',
      ],
      'defines': [ 'ENGINE_SUPPORTS_INT64', ],
      'all_dependent_settings': {
        'defines': [
        # SpiderMonkey bindings implements indexed deleters.
        'ENGINE_SUPPORTS_INDEXED_DELETERS',
        'ENGINE_SUPPORTS_INT64', ],
      },
    },

    {
      # Standalone executable for JS engine
      'target_name': 'mozjs',
      'type': '<(final_executable_type)',
      'sources': [
        'mozjs.cc',
      ],
      'dependencies': [
        ':engine',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/script/script.gyp:standalone_javascript_runner',
        '<(DEPTH)/third_party/mozjs/mozjs.gyp:mozjs_lib',
      ],
    },
  ],
}
