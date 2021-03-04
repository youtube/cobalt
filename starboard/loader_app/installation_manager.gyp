# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'installation_store_proto',
      'type': 'static_library',
      'sources': [
        'installation_store.pb.cc',
        'installation_store_store.pb.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'installation_manager',
      'type': 'static_library',
      'sources': [
        'installation_manager.cc',
        'installation_manager.h',
      ],
      'dependencies': [
        ':installation_store_proto',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'conditions': [
        ['sb_evergreen_compatible_lite != 1', {
          'dependencies': [
            '<(DEPTH)/starboard/loader_app/pending_restart.gyp:pending_restart',
          ],},
        ],
      ],
      'include_dirs': [
        # Get protobuf headers from the chromium tree.
        '<(DEPTH)/third_party/protobuf/src',
      ],
    },
    {
      'target_name': 'installation_manager_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'installation_manager_test.cc',
        '<(DEPTH)/starboard/common/test_main.cc',
      ],
      'conditions': [
        ['sb_evergreen_compatible_lite != 1', {
          'sources': [
            'pending_restart_test.cc',
          ],},
        ],
      ],
      'dependencies': [
         ':installation_manager',
         '<(DEPTH)/testing/gmock.gyp:gmock',
         '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'installation_manager_test_deploy',
      'type': 'none',
      'dependencies': [
        'installation_manager_test',
      ],
      'variables': {
        'executable_name': 'installation_manager_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
