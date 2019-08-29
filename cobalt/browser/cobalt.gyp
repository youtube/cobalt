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
  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'cobalt',
      'type': '<(final_executable_type)',
      'conditions': [
        ['sb_evergreen != 1', {
          'sources': [
            'main.cc',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/browser/browser.gyp:browser',
            '<(DEPTH)/net/net.gyp:net',
          ],
          'conditions': [
            ['cobalt_splash_screen_file != ""', {
              'dependencies': [
                '<(DEPTH)/cobalt/browser/splash_screen/splash_screen.gyp:copy_splash_screen',
              ],
            }],
          ],
        }],
        ['sb_evergreen == 1', {
          'ldflags': [
            '-Wl,--dynamic-list=<(DEPTH)/starboard/starboard.syms',
            '-Wl,--whole-archive',
            # TODO: Figure out how to take the gyp output from a variable.
            'obj/starboard/linux/x64x11/evergreen/libstarboard_platform.a',
            '-Wl,--no-whole-archive',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/browser/cobalt.gyp:cobalt_evergreen',
            '<(DEPTH)/starboard/starboard.gyp:starboard_full',
          ],
        }],
      ],
    },
    {
      'target_name': 'cobalt_deploy',
      'type': 'none',
      'dependencies': [
        'cobalt',
      ],
      'variables': {
        'executable_name': 'cobalt',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      # Convenience target to build cobalt and copy the demos into
      # content/data/test/cobalt/demos
      'target_name': 'cobalt_with_demos',
      'type': 'none',
      'dependencies': [
        'cobalt',
        '<(DEPTH)/cobalt/demos/demos.gyp:copy_demos',
      ],
    },
  ],
  'conditions': [
    ['sb_evergreen == 1', {
      'targets': [
        {
          'target_name': 'cobalt_evergreen',
          'type': 'shared_library',
          'libraries/': [
            ['exclude', '.*'],
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
            '<(DEPTH)/cobalt/browser/browser.gyp:browser',
            '<(DEPTH)/net/net.gyp:net',
            '<(DEPTH)/starboard/common/common.gyp:common',
          ],
          'sources': [
            'main.cc',
          ],
          'ldflags/': [
            ['exclude', '-Wl,--wrap=eglSwapBuffers'],
          ],
          'conditions': [
            ['clang and target_os not in ["tvos", "android", "orbis"] and sb_target_platform not in ["linux-x64x11-clang-3-6", "linux-x86x11"]', {
              'includes': [ '<(DEPTH)/cobalt/browser/cobalt_evergreen.gypi' ],
            }],
            ['cobalt_splash_screen_file != ""', {
              'dependencies': [
                '<(DEPTH)/cobalt/browser/splash_screen/splash_screen.gyp:copy_splash_screen',
              ],
            }],
          ],
        },
      ]
    }],
    ['build_snapshot_app_stats', {
      'targets': [
        {
          'target_name': 'snapshot_app_stats',
          'type': '<(final_executable_type)',
          'sources': [
            'snapshot_app_stats.cc',
          ],
          'dependencies': [
            'cobalt',
            '<(DEPTH)/cobalt/browser/browser.gyp:browser',
            '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
          ],
        },
        {
          'target_name': 'snapshot_app_stats_deploy',
          'type': 'none',
          'dependencies': [
            'snapshot_app_stats',
          ],
          'variables': {
            'executable_name': 'snapshot_app_stats',
          },
          'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
        },
      ]
    }],
    ['final_executable_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'cobalt_bin',
          'type': 'executable',
          'dependencies': [
            'cobalt',
          ],
        },
      ],
    }],
  ],
}
