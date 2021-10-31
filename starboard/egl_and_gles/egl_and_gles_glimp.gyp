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
  'targets': [
    {
      'target_name': 'egl_and_gles_implementation',
      'type': 'none',

      'dependencies': [
        '<(DEPTH)/glimp/glimp.gyp:glimp',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/glimp/glimp.gyp:glimp',
      ],
      'direct_dependent_settings': {
        'defines': [
          'GL_GLEXT_PROTOTYPES',
        ],
      },
      'conditions': [
        ['enable_vr==1', {
          'dependencies': [
            '<(DEPTH)/glimp/<(sb_target_platform)/glimp_platform.gyp:glimp_platform',
          ],
        }],
      ],
    },
  ],
}
