# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'storage_upgrade',
      'type': 'static_library',
      'sources': [
        '../store/storage.pb.h',
        'sql_vfs.cc',
        'sql_vfs.h',
        'upgrade.cc',
        'upgrade.h',
        'virtual_file.cc',
        'virtual_file.h',
        'virtual_file_system.cc',
        'virtual_file_system.h'
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/storage/storage_constants.gyp:storage_constants',
        '<(DEPTH)/cobalt/storage/store/store.gyp:memory_store',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/sql/sql.gyp:sql',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'storage_upgrade_test',
      'type': '<(gtest_target_type)',
      'sources': [
        '../store/storage.pb.h',
        'upgrade_test.cc',
      ],
      'dependencies': [
        'storage_upgrade',
        'storage_upgrade_copy_test_files',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },
    {
      'target_name': 'storage_upgrade_test_deploy',
      'type': 'none',
      'dependencies': [
        'storage_upgrade_test',
      ],
      'variables': {
        'executable_name': 'storage_upgrade_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'storage_upgrade_copy_test_files',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/storage/store_upgrade/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/storage/store_upgrade/testdata/',
      },
      'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
    },
  ],
}
