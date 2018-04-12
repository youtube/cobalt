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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'cobalt',
      'type': '<(final_executable_type)',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
      ],
      'conditions': [
        ['cobalt_enable_lib == 1', {
          'sources': [
            'lib/cobalt.def',
            'lib/main.cc',
          ],
        }, {
          'sources': [
            'main.cc',
          ],
        }],
        ['cobalt_copy_debug_console == 1', {
          'dependencies': [
            '<(DEPTH)/cobalt/browser/browser.gyp:browser_copy_debug_console',
          ],
        }],
        ['cobalt_splash_screen_file != ""', {
          'dependencies': [
            '<(DEPTH)/cobalt/browser/splash_screen/splash_screen.gyp:copy_splash_screen',
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
