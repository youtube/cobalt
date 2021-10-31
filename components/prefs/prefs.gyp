# Copyright 2019 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'prefs',
      'type': 'static_library',
      'sources': [
        'command_line_pref_store.cc',
        'command_line_pref_store.h',
        'default_pref_store.cc',
        'default_pref_store.h',
        'in_memory_pref_store.cc',
        'in_memory_pref_store.h',
        'json_pref_store.cc',
        'json_pref_store.h',
        'overlay_user_pref_store.cc',
        'overlay_user_pref_store.h',
        'persistent_pref_store.cc',
        'persistent_pref_store.h',
        'pref_change_registrar.cc',
        'pref_change_registrar.h',
        'pref_filter.h',
        'pref_member.cc',
        'pref_member.h',
        'pref_notifier.h',
        'pref_notifier_impl.cc',
        'pref_notifier_impl.h',
        'pref_observer.h',
        'pref_registry.cc',
        'pref_registry.h',
        'pref_registry_simple.cc',
        'pref_registry_simple.h',
        'pref_service.cc',
        'pref_service.h',
        'pref_service_factory.cc',
        'pref_service_factory.h',
        'pref_store.cc',
        'pref_store.h',
        'pref_value_map.cc',
        'pref_value_map.h',
        'pref_value_store.cc',
        'pref_value_store.h',
        'prefs_export.h',
        'scoped_user_pref_update.cc',
        'scoped_user_pref_update.h',
        'value_map_pref_store.cc',
        'value_map_pref_store.h',
        'writeable_pref_store.cc',
        'writeable_pref_store.h',
      ],
     'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/util/values/values_util.gyp:values_util',
      ],
     'defines': [
       'COMPONENTS_PREFS_IMPLEMENTATION',
     ],
    },
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'sources': [
        'mock_pref_change_callback.cc',
        'mock_pref_change_callback.h',
        'pref_store_observer_mock.cc',
        'pref_store_observer_mock.h',
        'testing_pref_service.cc',
        'testing_pref_service.h',
        'testing_pref_store.cc',
        'testing_pref_store.h',
      ],
      'dependencies': [
        'prefs',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ]
}
