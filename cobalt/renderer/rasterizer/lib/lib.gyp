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
      'target_name': 'external_rasterizer',
      'type': 'static_library',
      'includes': [
        '../../renderer_parameters_setup.gypi',
      ],
      'sources': [
        'external_rasterizer.cc',
        'renderer_module_default_options_lib.cc'
      ],
       'dependencies': [
         '<(DEPTH)/base/base.gyp:base',
         '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
         '<(DEPTH)/cobalt/renderer/rasterizer/egl/rasterizer.gyp:hardware_rasterizer',
         '<(DEPTH)/cobalt/renderer/rasterizer/skia/common.gyp:common',
         '<(DEPTH)/cobalt/renderer/rasterizer/skia/rasterizer.gyp:hardware_rasterizer',
         '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
         '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
       ],
      'conditions': [
        ['enable_map_to_mesh == 1', {
          'defines' : ['ENABLE_MAP_TO_MESH'],
        }],
        ['gl_type == "angle"', {
          'dependencies': [
             '<(DEPTH)/third_party/angle/angle.gyp:libANGLE',
           ],
        }],
      ],
    }
  ],
}
