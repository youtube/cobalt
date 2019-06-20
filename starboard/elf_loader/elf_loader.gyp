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
      'target_name': 'elf_loader',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
        'src/src/',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'sources': [
        'elf_header.h',
        'elf_header.cc',
        'elf_hash_table.h',
        'elf_hash_table.cc',
        'elf_loader.h',
        'elf_loader.cc',
        'elf_loader_impl.h',
        'elf_loader_impl.cc',
        'exported_symbols.cc',
        'file.h',
        'file_impl.h',
        'file_impl.cc',
        'gnu_hash_table.h',
        'gnu_hash_table.cc',
        'dynamic_section.h',
        'dynamic_section.cc',
        'program_table.h',
        'program_table.cc',
        'relocations.h',
        'relocations.cc',
      ],
    },
    {
      'target_name': 'elf_loader_sandbox',
      'type': '<(final_executable_type)',
      'include_dirs': [
        'src/include',
        'src/src/',
      ],
      'dependencies': [
        'elf_loader',
        '<(DEPTH)/starboard/starboard.gyp:starboard_full',
      ],
      'sources': [
        'sandbox.cc',
      ],
    },
    {
      'target_name': 'elf_loader_test',
      'type': '<(gtest_target_type)',
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
        'elf_loader_test.cc',
        'elf_header_test.cc',
        'dynamic_section_test.cc',
        'program_table_test.cc',
        'relocations_test.cc',
      ],
      'dependencies': [
        'elf_loader',
        '<(DEPTH)/starboard/starboard.gyp:starboard_full',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ]
}
