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

# The Starboard "all" target, which includes all interesting targets for the
# Starboard project.

{
  'variables': {
    'has_platform_tests%' : '<!(python ../build/file_exists.py <(DEPTH)/<(starboard_path)/starboard_platform_tests.gyp)',
  },
  'targets': [
    {
      # Note that this target must be in a separate GYP file from starboard.gyp,
      # or else it produces a GYP file loop (which is not allowed by GYP).
      'target_name': 'starboard_all',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/starboard/client_porting/eztime/eztime.gyp:*',
        '<(DEPTH)/starboard/client_porting/icu_init/icu_init.gyp:*',
        '<(DEPTH)/starboard/client_porting/poem/poem.gyp:*',
        '<(DEPTH)/starboard/examples/examples.gyp:*',
        '<(DEPTH)/starboard/nplb/blitter_pixel_tests/blitter_pixel_tests.gyp:*',
        '<(DEPTH)/starboard/nplb/nplb.gyp:*',
        '<(DEPTH)/starboard/starboard.gyp:*',
      ],
      'conditions': [
        ['has_platform_tests=="True"', {
          'dependencies': [
            '<(DEPTH)/<(starboard_path)/starboard_platform_tests.gyp:*',
          ],
        }],
      ],
    },
  ],
}
