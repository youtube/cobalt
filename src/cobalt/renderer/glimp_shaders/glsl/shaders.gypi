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

# This file defines 'glsl_keys', which is an input to glimp's
# map_glsl_shaders.gypi tool used to map GLSL shaders to platform-specific
# shaders, mapping them by filename.  The 'glsl_keys' variable defined here
# lists all GLSL shaders intended to be used by Cobalt.  Make sure that this
# is included before including 'glimp/map_glsl_shaders.gypi'.

{
  'variables': {
    'glsl_shaders_dir': '<(DEPTH)/cobalt/renderer/glimp_shaders/glsl',
    'glsl_shaders': [
      # A simple shader allowing for full-screen quad blitting, used to enable
      # the transfer of a software-rasterized image to the display.
      'fragment_position_and_texcoord.glsl',
      'fragment_skia_alpha_texcoords_and_color.glsl',
      'fragment_skia_alpha_texcoords_and_color_2.glsl',
      'fragment_skia_antialiased_circle.glsl',
      'fragment_skia_antialiased_color_only.glsl',
      'fragment_skia_antialiased_oval.glsl',
      'fragment_skia_bounded_x_convolution_filtered_texture.glsl',
      'fragment_skia_bounded_y_convolution_filtered_texture.glsl',
      'fragment_skia_circle_masked_texture.glsl',
      'fragment_skia_circle_masked_texture_domain.glsl',
      'fragment_skia_circle_outside_masked_circle.glsl',
      'fragment_skia_color_filtered_texture.glsl',
      'fragment_skia_color_only.glsl',
      'fragment_skia_convolution_filtered_texture.glsl',
      'fragment_skia_distance_field_texture.glsl',
      'fragment_skia_distance_field_texture_non_similar.glsl',
      'fragment_skia_ellipse_masked_texture.glsl',
      'fragment_skia_ellipse_masked_texture_domain.glsl',
      'fragment_skia_linear_gradient_many_colors.glsl',
      'fragment_skia_linear_gradient_three_colors.glsl',
      'fragment_skia_linear_gradient_two_colors.glsl',
      'fragment_skia_nv12.glsl',
      'fragment_skia_radial_gradient_many_colors.glsl',
      'fragment_skia_radial_gradient_three_colors.glsl',
      'fragment_skia_radial_gradient_two_colors.glsl',
      'fragment_skia_rect_blur.glsl',
      'fragment_skia_rect_outside_masked_color.glsl',
      'fragment_skia_rect_outside_masked_rect_blur.glsl',
      'fragment_skia_rect_outside_masked_texture.glsl',
      'fragment_skia_texcoords_and_color.glsl',
      'fragment_skia_texcoords_and_color_with_edge_clamped_texels.glsl',
      'fragment_skia_texture_domain.glsl',
      'fragment_skia_texture_domain_masked_texture_domain.glsl',
      'fragment_skia_texture_masked_texture.glsl',
      'fragment_skia_yuv.glsl',
      'vertex_position_and_texcoord.glsl',
      'vertex_skia_antialiased_circle.glsl',
      'vertex_skia_antialiased_color_only.glsl',
      'vertex_skia_antialiased_oval.glsl',
      'vertex_skia_color_only.glsl',
      'vertex_mesh.glsl',
      'vertex_skia_texcoords_and_color.glsl',
      'vertex_skia_texcoords_and_color_with_texcoord_matrix.glsl',
      'vertex_skia_texcoords_derived_from_position.glsl',
      'vertex_skia_two_texcoords_and_color_with_texcoord_matrices.glsl',
      'vertex_skia_two_texcoords_derived_from_position.glsl',
      'vertex_skia_yuv.glsl',
    ],
  }
}
