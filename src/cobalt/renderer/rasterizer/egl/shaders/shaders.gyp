# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
    'output_dir': '<(SHARED_INTERMEDIATE_DIR)/egl',
    'shader_impl_header': '<(output_dir)/generated_shader_impl.h',
    'shader_impl_source': '<(output_dir)/generated_shader_impl.cc',
    'generate_class_script': '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/generate_shader_impl.py',
    'shader_sources': [
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color_blur.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color_blur_rrects.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color_include.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color_texcoord.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_color_texcoord_yuv3.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_opacity_texcoord1d.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_rcorner_color.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_rcorner2_color.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_rcorner_texcoord_color.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_rcorner_texcoord_color_yuv3.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_texcoord.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/fragment_texcoord_yuv3.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/function_gaussian_integral.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/function_is_outside_rcorner.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/image_sampler_rgba.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/image_sampler_rgba_with_clamp.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/image_sampler_yuv3_with_clamp.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/image_sampler_yuv3.inc',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_color.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_color_offset.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_color_texcoord.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_offset.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_offset_rcorner.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_rcorner.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_rcorner2.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_rcorner_texcoord.glsl',
      '<(DEPTH)/cobalt/renderer/rasterizer/egl/shaders/vertex_texcoord.glsl',
    ],
  },

  'targets': [
    {
      'target_name': 'generate_shader_impl',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'variables': {
        'input_shader_files_file': '<|(generate_shader_impl_inputs.tmp <@(shader_sources))',
      },

      'actions': [
        {
          # Parse shader files to create a header and source file which contain
          # classes encapsulating the shader attributes, uniforms, and source.
          'action_name': 'create_shader_classes',
          'inputs': [
            '<(generate_class_script)',
            '<(input_shader_files_file)',
            '<@(shader_sources)',
          ],
          'outputs': [
            '<(shader_impl_header)',
            '<(shader_impl_source)',
          ],
          'action': [
            'python2',
            '<(generate_class_script)',
            '<(shader_impl_header)',
            '<(shader_impl_source)',
            '<(input_shader_files_file)',
          ],
          'message': 'Generating <(shader_impl_header) and <(shader_impl_source)',
        },
      ],
    },
    {
      'target_name': 'shaders',
      'type': 'static_library',
      # We're depending on a target which generates a header file, so make sure other
      # targets which need the header file know this.
      'hard_dependency': 1,

      'sources' : [
        '<(shader_impl_header)',
        '<(shader_impl_source)',
      ],

      'dependencies': [
        'generate_shader_impl',
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ],
}
