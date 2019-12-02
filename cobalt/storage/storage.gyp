# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
        'savegame_fake.cc',
        'savegame_starboard.cc',
        'storage_manager.cc',
        'storage_manager.h',
        'upgrade/upgrade_reader.cc',
        'upgrade/upgrade_reader.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/storage/store/store.gyp:memory_store',
        '<(DEPTH)/cobalt/storage/store_upgrade/upgrade.gyp:storage_upgrade',
        '<(DEPTH)/net/net.gyp:net',
      ],
    },
    {
      'target_name': 'storage_test',
      'type': '<(gtest_target_type)',
      'defines': [
        'GMOCK_NO_MOVE_MOCK',
      ],
      'sources': [
        'savegame_test.cc',
        'storage_manager_test.cc',
        'upgrade/storage_upgrade_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'storage',
        'storage_upgrade_copy_test_data',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'storage_upgrade_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/storage/upgrade/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/storage/upgrade/testdata/',
      },
      'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
    },
  ],
}
