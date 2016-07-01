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
        'allocator.h',
        'allocator_decorator.h',
        'clock.h',
        'cobalt_paths.h',
        'compiler.h',
        'console_commands.cc',
        'console_commands.h',
        'c_val.cc',
        'c_val.h',
        'do_main.h',
        'do_main_starboard.h',
        'event.h',
        'event_dispatcher.cc',
        'event_dispatcher.h',
        'fixed_no_free_allocator.h',
        'init_cobalt.cc',
        'init_cobalt.h',
        'localized_strings.cc',
        'localized_strings.h',
        'log_message_handler.cc',
        'log_message_handler.h',
        'math.cc',
        'math.h',
        'memory_pool.cc',
        'memory_pool.h',
        'pointer_arithmetic.h',
        'poller.h',
        'polymorphic_downcast.h',
        'polymorphic_equatable.h',
        'reuse_allocator.cc',
        'reuse_allocator.h',
        'sanitizer_options.cc',
        'source_location.h',
        'stop_watch.cc',
        'stop_watch.h',
        'token.cc',
        'token.h',
        'tokens.cc',
        'tokens.h',
        'type_id.h',
        'unicode/character.cc',
        'unicode/character.h',
        'unicode/character_names.h',
        'unused.h',
        'user_log.cc',
        'user_log.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/cobalt/deprecated/deprecated.gyp:platform_delegate',
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
        'c_val_test.cc',
        'fixed_no_free_allocator_test.cc',
        'reuse_allocator_test.cc',
        'token_test.cc',
      ],
      'dependencies': [
        'base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
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
      'includes': [ '../build/deploy.gypi' ],
    },

    {
      'target_name': 'reuse_allocator_benchmark',
      'type': '<(gtest_target_type)',
      'sources': [
        'reuse_allocator_benchmark.cc',
      ],
      'dependencies': [
        'base',
      ],
      'actions': [
        {
          'action_name': 'reuse_allocator_benchmark_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/base/testdata/',
            ],
            'output_dir': 'cobalt/base/testdata',
          },
          'includes': ['../build/copy_test_data.gypi'],
        }
      ],
    },
    {
      'target_name': 'reuse_allocator_benchmark_deploy',
      'type': 'none',
      'dependencies': [
        'reuse_allocator_benchmark',
      ],
      'variables': {
        'executable_name': 'reuse_allocator_benchmark',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
