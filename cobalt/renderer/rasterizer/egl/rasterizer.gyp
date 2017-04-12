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
      'target_name': 'software_rasterizer',
      'type': 'static_library',

      'sources': [
        'software_rasterizer.cc',
        'software_rasterizer.h',
        'textured_mesh_renderer.cc',
        'textured_mesh_renderer.h',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/software_rasterizer.gyp:software_rasterizer',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
      ],
    },
    {
      'target_name': 'hardware_rasterizer',
      'type': 'static_library',

      'sources': [
        'draw_depth_stencil.h',
        'draw_depth_stencil.cc',
        'draw_object.h',
        'draw_object.cc',
        'draw_object_manager.h',
        'draw_object_manager.cc',
        'draw_poly_color.h',
        'draw_poly_color.cc',
        'draw_rect_color_texture.h',
        'draw_rect_color_texture.cc',
        'draw_rect_shadow_spread.h',
        'draw_rect_shadow_spread.cc',
        'draw_rect_shadow_blur.h',
        'draw_rect_shadow_blur.cc',
        'draw_rect_texture.h',
        'draw_rect_texture.cc',
        'graphics_state.h',
        'graphics_state.cc',
        'hardware_rasterizer.cc',
        'hardware_rasterizer.h',
        'offscreen_target_manager.h',
        'offscreen_target_manager.cc',
        'rect_allocator.h',
        'rect_allocator.cc',
        'render_tree_node_visitor.h',
        'render_tree_node_visitor.cc',
        'shader_base.h',
        'shader_base.cc',
        'shader_program.h',
        'shader_program.cc',
        'shader_program_manager.h',
        'shader_program_manager.cc',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
        '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/shaders.gyp:shaders',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/rasterizer.gyp:hardware_rasterizer',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
      ],
    },
  ],
}
