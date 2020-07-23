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
  'variables': {
    'common_elf_loader_sources': [
        'elf_header.h',
        'elf_header.cc',
        'elf_hash_table.h',
        'elf_hash_table.cc',
        'elf_loader.h',
        'elf_loader.cc',
        'elf_loader_switches.h',
        'elf_loader_switches.cc',
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
    'elf_loader_impl_sources': [
        'elf_loader_impl.h',
        'elf_loader_impl.cc',
    ],
    'elf_loader_sys_sources': [
        'elf_loader_sys_impl.h',
        'elf_loader_sys_impl.cc',
    ]
  },
  'targets': [
    {
      'target_name': 'elf_loader',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
        'src/src/',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/elf_loader/evergreen_config.gyp:evergreen_config',
        '<(DEPTH)/starboard/elf_loader/evergreen_info.gyp:evergreen_info',
        '<(DEPTH)/starboard/starboard.gyp:starboard_base',
      ],
      'conditions': [
        ['sb_evergreen_compatible == 1', {
          'dependencies': [
            '<(DEPTH)/third_party/crashpad/wrapper/wrapper.gyp:crashpad_wrapper',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/third_party/crashpad/wrapper/wrapper.gyp:crashpad_wrapper_stub',
          ],
        }],
      ],
      'sources': [
        '<@(common_elf_loader_sources)',
        '<@(elf_loader_impl_sources)',
      ],
    },
    {
      # System loader based on dlopen/dlsym.
      # Should be used only for debugging/troubleshooting.
      'target_name': 'elf_loader_sys',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
        'src/src/',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/elf_loader/evergreen_config.gyp:evergreen_config',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'sources': [
        '<@(common_elf_loader_sources)',
        '<@(elf_loader_sys_sources)',
      ],
      'conditions': [
        ['sb_evergreen_compatible == 1', {
          'dependencies': [
            '<(DEPTH)/third_party/crashpad/wrapper/wrapper.gyp:crashpad_wrapper',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/third_party/crashpad/wrapper/wrapper.gyp:crashpad_wrapper_stub',
          ],
        }],
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
        '<(DEPTH)/cobalt/content/fonts/fonts.gyp:copy_font_data',
        '<(DEPTH)/starboard/starboard.gyp:starboard_full',
        # TODO: Remove this dependency once MediaSession is migrated to use CobaltExtensions.
        '<@(cobalt_platform_dependencies)',
      ],
      'sources': [
        'sandbox.cc',
      ],
    },
    {
      'target_name': 'elf_loader_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'elf_loader_sandbox',
      ],
      'variables': {
        'executable_name': 'elf_loader_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      # To properly function the system loader requires the starboard
      # symbols to be exported from the binary.
      # To allow symbols to be exported remove the '-fvisibility=hidden' flag
      # from your compiler_flags.gypi. For Linux this would be:
      #   starboard/linux/shared/compiler_flags.gypi
      # Example run:
      # export LD_LIBRARY_PATH=.
      # ./elf_loader_sys_sandbox --evergreen_library=app/cobalt/lib/libcobalt.so --evergreen_content=app/cobalt/content
      'target_name': 'elf_loader_sys_sandbox',
      'type': '<(final_executable_type)',
      'include_dirs': [
        'src/include',
        'src/src/',
      ],
      'dependencies': [
        'elf_loader_sys',
        '<(DEPTH)/starboard/starboard.gyp:starboard_full',
      ],
      'sources': [
        'sandbox.cc',
      ],
      'ldflags': [
        '-Wl,--dynamic-list=<(DEPTH)/starboard/starboard.syms',
        '-ldl' ,
      ],
    },
    {
      'target_name': 'elf_loader_test',
      'type': '<(gtest_target_type)',
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard_full',
        # TODO: Remove this dependency once MediaSession is migrated to use CobaltExtensions.
        '<@(cobalt_platform_dependencies)',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'conditions': [
        ['target_arch in ["x86", "x64", "arm", "arm64"] ', {
          'sources': [
            'elf_loader_test.cc',
            'elf_header_test.cc',
            'dynamic_section_test.cc',
            'program_table_test.cc',
            'relocations_test.cc',
          ],
          'dependencies': [
            'elf_loader',
          ],
        }],
      ],
    },
    {
      'target_name': 'elf_loader_test_deploy',
      'type': 'none',
      'dependencies': [
        'elf_loader_test',
      ],
      'variables': {
        'executable_name': 'elf_loader_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ]
}
