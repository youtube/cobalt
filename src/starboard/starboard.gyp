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

# The common "starboard" target. Any target that depends on Starboard should
# depend on this common target, and not any of the specific "starboard_platform"
# targets.

{
  'targets': [
    {
      'target_name': 'starboard',
      'type': 'none',
      'conditions': [
        ['sb_evergreen == 1', {
          'dependencies': [
            '<(DEPTH)/starboard/client_porting/cwrappers/cwrappers.gyp:cwrappers',
            '<(DEPTH)/starboard/client_porting/eztime/eztime.gyp:eztime',
            '<(DEPTH)/starboard/elf_loader/sabi_string.gyp:sabi_string',
            '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
            '<(DEPTH)/third_party/llvm-project/compiler-rt/compiler-rt.gyp:compiler_rt',
            '<(DEPTH)/third_party/llvm-project/libcxx/libcxx.gyp:cxx',
            '<(DEPTH)/third_party/llvm-project/libcxxabi/libcxxabi.gyp:cxxabi',
            '<(DEPTH)/third_party/llvm-project/libunwind/libunwind.gyp:unwind_evergreen',
            '<(DEPTH)/third_party/musl/musl.gyp:c',
          ],
        }, {
          'dependencies': [
            'starboard_full',
          ],
        }],
      ],
    },
    {
      'target_name': 'starboard_full',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/<(starboard_path)/starboard_platform.gyp:starboard_platform',
        '<(DEPTH)/starboard/client_porting/cwrappers/cwrappers.gyp:cwrappers',
        '<(DEPTH)/starboard/client_porting/eztime/eztime.gyp:eztime',
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
        'common/common.gyp:common',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/<(starboard_path)/starboard_platform.gyp:starboard_platform',
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
        ['final_executable_type=="shared_library"', {
          'all_dependent_settings': {
            'target_conditions': [
              ['_type=="executable" and _toolset=="target"', {
                'sources': [
                  '<(DEPTH)/starboard/shared/starboard/shared_main_adapter.cc',
                ],
              }],
            ],
          },
        }],
      ],
    },
  ],
}
