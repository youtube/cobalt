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
  'variables': {
    'cobalt_code': 1,
    'static_contents_dir': '<(DEPTH)/lbshell/content',
  },
  'targets': [
    {
      'target_name': 'storage',
      'type': 'static_library',
      'sources': [
        'savegame.cc',
        'savegame.h',
        'sql_vfs.cc',
        'sql_vfs.h',
        'storage_manager.cc',
        'storage_manager.h',
        'virtual_file.cc',
        'virtual_file.h',
        'virtual_file_system.cc',
        'virtual_file_system.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/sql/sql.gyp:sql',
      ],
      'conditions': [
        ['target_arch=="ps3"', {
          'sources': [
            'savegame_ps3.cc',
          ],
          'copies': [
          {
            'destination': '<(PRODUCT_DIR)/content/data',
            'files': ['<(static_contents_dir)/platform/ps3/USRDIR/SAVE_ICON.PNG'],
          }],
        }],
        ['actual_target_arch in ["linux", "win"]', {
          'sources': [
            'savegame_file.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'storage_test_utils',
      'type': 'static_library',
      'sources': [
          'savegame_fake.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'storage_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'savegame_test.cc',
        'storage_manager_test.cc',
        'virtual_file_system_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'storage',
        'storage_test_utils',
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
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
