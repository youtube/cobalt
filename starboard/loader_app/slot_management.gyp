# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'slot_management',
      'type': 'static_library',
      'sources': [
        'slot_management.cc',
        'slot_management.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/elf_loader/sabi_string.gyp:sabi_string',
        '<(DEPTH)/starboard/elf_loader/elf_loader.gyp:elf_loader',
        '<(DEPTH)/starboard/loader_app/app_key_files.gyp:app_key_files',
        '<(DEPTH)/starboard/loader_app/drain_file.gyp:drain_file',
        '<(DEPTH)/starboard/loader_app/installation_manager.gyp:installation_manager',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
    {
      'target_name': 'slot_management_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'slot_management_test.cc',
        '<(DEPTH)/starboard/common/test_main.cc',
      ],
      'dependencies': [
         ':slot_management',
         '<(DEPTH)/testing/gmock.gyp:gmock',
         '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'slot_management_test_deploy',
      'type': 'none',
      'dependencies': [
        'slot_management_test',
      ],
      'variables': {
        'executable_name': 'slot_management_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
