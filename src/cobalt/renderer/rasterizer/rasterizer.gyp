# Copyright 2016 Google Inc. All Rights Reserved.
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
      'target_name': 'rasterizer',
      'type': 'none',

      'conditions': [
        ['rasterizer_type == "stub"', {
          'dependencies': [
            '<(DEPTH)/cobalt/renderer/rasterizer/stub/rasterizer.gyp:rasterizer',
          ],
        }, {
          'conditions': [
            ['gl_type != "none"', {
              'dependencies': [
                '<(DEPTH)/cobalt/renderer/rasterizer/skia/rasterizer.gyp:hardware_rasterizer',
                '<(DEPTH)/cobalt/renderer/rasterizer/egl/rasterizer.gyp:software_rasterizer',
              ],
            }],
            ['OS=="starboard"', {
              'dependencies': [
                '<(DEPTH)/cobalt/renderer/rasterizer/blitter/rasterizer.gyp:hardware_rasterizer',
                '<(DEPTH)/cobalt/renderer/rasterizer/blitter/rasterizer.gyp:software_rasterizer',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
