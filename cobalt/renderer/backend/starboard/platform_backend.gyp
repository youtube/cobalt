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
  'targets': [
    {
      'target_name': 'renderer_platform_backend',
      'type': 'static_library',

      'conditions': [
        ['gl_type == "none"', {
          'sources': [
            'default_graphics_system_blitter.cc',
          ],

          'includes': [
            '../blitter/blitter_backend.gypi',
          ],
        }, {
          'sources': [
            'default_graphics_system_egl.cc',
          ],

          'includes': [
            '../egl/egl_backend.gypi',
          ],
        }],
      ],
    },
  ],
}
