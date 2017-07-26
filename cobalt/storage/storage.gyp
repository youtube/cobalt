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
  'includes': [ '../build/contents_dir.gypi' ],

  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'storage',
      'type': 'static_library',
      'sources': [
        'savegame.cc',
        'savegame.h',
        'savegame_thread.cc',
        'savegame_thread.h',
        'sql_vfs.cc',
        'sql_vfs.h',
        'savegame_fake.cc',
        'savegame_starboard.cc',
        'storage_manager.cc',
        'storage_manager.h',
        'upgrade/upgrade_reader.cc',
        'upgrade/upgrade_reader.h',
        'virtual_file.cc',
        'virtual_file.h',
        'virtual_file_system.cc',
        'virtual_file_system.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/sql/sql.gyp:sql',
      ],
    },
    {
      'target_name': 'storage_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'savegame_test.cc',
        'storage_manager_test.cc',
        'upgrade/storage_upgrade_test.cc',
        'virtual_file_system_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'storage',
        'storage_upgrade_copy_test_data',
      ],
    },
    {
      'target_name': 'storage_test_deploy',
      'type': 'none',
      'dependencies': [
        'storage_test',
      ],
      'variables': {
        'executable_name': 'storage_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'storage_upgrade_copy_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'storage_upgrade_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/storage/upgrade/testdata/',
            ],
            'output_dir': 'cobalt/storage/upgrade/testdata/',
          },
          'includes': [ '../build/copy_test_data.gypi' ],
        },
      ],
    },
  ],
}
