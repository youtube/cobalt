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
  'targets': [
    {
      'target_name': 'base',
      # Override library name, to avoid conflicting with Chromium base when
      # generating PDBs.
      'product_name': 'cobalt_base',
      'type': 'static_library',
      'sources': [
        'accessibility_changed_event.h',
        'address_sanitizer.h',
        'camera_transform.h',
        'clock.h',
        'cobalt_paths.h',
        'compiler.h',
        'console_commands.cc',
        'console_commands.h',
        'c_val.cc',
        'c_val.h',
        'c_val_collection_entry_stats.h',
        'c_val_collection_timer_stats.h',
        'c_val_time_interval_entry_stats.h',
        'c_val_time_interval_timer_stats.h',
        'deep_link_event.h',
        'do_main.h',
        'do_main_starboard.h',
        'event.h',
        'event_dispatcher.cc',
        'event_dispatcher.h',
        'get_application_key.cc',
        'get_application_key.h',
        'init_cobalt.cc',
        'init_cobalt.h',
        'language.cc',
        'language.h',
        'localized_strings.cc',
        'localized_strings.h',
        'log_message_handler.cc',
        'log_message_handler.h',
        'math.h',
        'message_queue.h',
        'on_screen_keyboard_hidden_event.h',
        'on_screen_keyboard_shown_event.h',
        'path_provider.cc',
        'path_provider.h',
        'poller.h',
        'polymorphic_downcast.h',
        'polymorphic_equatable.h',
        'ref_counted_lock.h',
        'source_location.h',
        'startup_timer.cc',
        'startup_timer.h',
        'stop_watch.cc',
        'stop_watch.h',
        'token.cc',
        'token.h',
        'tokens.cc',
        'tokens.h',
        'type_id.h',
        'window_size_changed_event.h',
        'unicode/character.cc',
        'unicode/character.h',
        'unicode/character_values.h',
        'unused.h',
        'user_log.cc',
        'user_log.h',
        'version_compatibility.cc',
        'version_compatibility.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libxml/src/include',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'conditions': [
        ['OS != "starboard"', {
          'includes': [
            'copy_i18n_data.gypi',
          ],
        }],
      ],
    },
    {
      'target_name': 'base_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'c_val_collection_entry_stats_test.cc',
        'c_val_collection_timer_stats_test.cc',
        'c_val_test.cc',
        'c_val_time_interval_entry_stats_test.cc',
        'c_val_time_interval_timer_stats_test.cc',
        'token_test.cc',
        'fixed_size_lru_cache_test.cc',
      ],
      'dependencies': [
        'base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'base_test_deploy',
      'type': 'none',
      'dependencies': [
        'base_test',
      ],
      'variables': {
        'executable_name': 'base_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
