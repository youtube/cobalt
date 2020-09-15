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
    'common_loader_app_sources': [
      'loader_app.cc',
      'loader_app_switches.h',
      'loader_app_switches.cc',
      'system_get_extension_shim.h',
      'system_get_extension_shim.cc',
    ],
    'common_loader_app_dependencies': [
      '<(DEPTH)/starboard/elf_loader/sabi_string.gyp:sabi_string',
      '<(DEPTH)/starboard/loader_app/app_key.gyp:app_key',
      '<(DEPTH)/starboard/loader_app/installation_manager.gyp:installation_manager',
      '<(DEPTH)/starboard/loader_app/slot_management.gyp:slot_management',
      '<(DEPTH)/starboard/starboard.gyp:starboard',
    ],
  },
  'targets': [
    {
      'target_name': 'loader_app',
      'type': '<(final_executable_type)',
      'conditions': [
        ['target_arch in ["x86", "x64", "arm", "arm64"] ', {
          'sources': [
            '<@(common_loader_app_sources)',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/content/fonts/fonts.gyp:copy_font_data',
            '<(DEPTH)/starboard/elf_loader/elf_loader.gyp:elf_loader',
            '<@(common_loader_app_dependencies)',
            # TODO: Remove this dependency once MediaSession is migrated to use CobaltExtensions.
            '<@(cobalt_platform_dependencies)',
          ],
        }],
      ],
    },
    {
      'target_name': 'loader_app_deploy',
      'type': 'none',
      'dependencies': [
        'loader_app',
      ],
      'variables': {
        'executable_name': 'loader_app',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'loader_app_sys',
      'type': '<(final_executable_type)',
      'conditions': [
        ['target_arch in ["x86", "x64", "arm", "arm64"] ', {
          'sources': [
            '<@(common_loader_app_sources)',
          ],
          'dependencies': [
            '<(DEPTH)/starboard/elf_loader/elf_loader.gyp:elf_loader_sys',
            '<@(common_loader_app_dependencies)',
            # TODO: Remove this dependency once MediaSession is migrated to use CobaltExtensions.
            '<@(cobalt_platform_dependencies)',
          ],
          'ldflags': [
            '-Wl,--dynamic-list=<(DEPTH)/starboard/starboard.syms',
            '-ldl' ,
          ],
        }],
      ],
    },
    {
      'target_name': 'loader_app_sys_deploy',
      'type': 'none',
      'dependencies': [
        'loader_app',
      ],
      'variables': {
        'executable_name': 'loader_app_sys',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'loader_app_tests_deploy',
      'type': 'none',
      'dependencies': [
        'loader_app',
        '<(DEPTH)/starboard/loader_app/app_key.gyp:app_key_test_deploy',
        '<(DEPTH)/starboard/loader_app/app_key_files.gyp:app_key_files_test_deploy',
        '<(DEPTH)/starboard/loader_app/drain_file.gyp:drain_file_test_deploy',
        '<(DEPTH)/starboard/loader_app/installation_manager.gyp:installation_manager_test_deploy',
        '<(DEPTH)/starboard/loader_app/slot_management.gyp:slot_management_test_deploy',
      ],
    },
  ],
}
