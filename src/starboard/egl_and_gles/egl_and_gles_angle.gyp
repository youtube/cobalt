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
    'enable_d3d11_feature_level_11%': 0,
  },
  'targets': [
    {
      'target_name': 'egl_and_gles_implementation',
      'type': 'none',

      'dependencies': [
        '<(DEPTH)/third_party/angle/angle.gyp:libEGL',
        '<(DEPTH)/third_party/angle/angle.gyp:libGLESv2',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/angle/include',
        ],
      },
      'conditions': [
        # ANGLE supports GLES3 on Windows only if DirectX11 feature level 11 is
        # supported.
        ['target_os=="win" and enable_d3d11_feature_level_11==1', {
          'direct_dependent_settings': {
            'defines': [
              'GL_GLEXT_PROTOTYPES',
            ],
          },
        }]
      ]
    },
  ],
}
