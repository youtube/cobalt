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
  'sources': [
    'egl/attrib_map.cc',
    'egl/attrib_map.h',
    'egl/config.cc',
    'egl/config.h',
    'egl/display.cc',
    'egl/display.h',
    'egl/display_impl.h',
    'egl/display_registry.cc',
    'egl/display_registry.h',
    'egl/error.cc',
    'egl/error.h',
    'egl/get_proc_address_impl.h',
    'egl/scoped_egl_lock.cc',
    'egl/scoped_egl_lock.h',
    'egl/surface.cc',
    'egl/surface.h',
    'egl/surface_impl.h',
    'entry_points/egl.cc',
    'entry_points/egl_ext.cc',
    'entry_points/gles_2_0.cc',
    'entry_points/gles_2_0_ext.cc',
    'entry_points/gles_3_0.cc',
    'gles/blend_state.h',
    'gles/buffer.cc',
    'gles/buffer.h',
    'gles/buffer_impl.h',
    'gles/context.cc',
    'gles/context.h',
    'gles/context_impl.h',
    'gles/convert_pixel_data.cc',
    'gles/convert_pixel_data.h',
    'gles/cull_face_state.h',
    'gles/draw_mode.h',
    'gles/draw_state.cc',
    'gles/draw_state.h',
    'gles/framebuffer.cc',
    'gles/framebuffer.h',
    'gles/index_data_type.h',
    'gles/pixel_format.cc',
    'gles/pixel_format.h',
    'gles/program.cc',
    'gles/program.h',
    'gles/program_impl.h',
    'gles/ref_counted_resource_map.h',
    'gles/renderbuffer.cc',
    'gles/renderbuffer.h',
    'gles/resource_manager.cc',
    'gles/resource_manager.h',
    'gles/sampler.h',
    'gles/shader.cc',
    'gles/shader.h',
    'gles/shader_impl.h',
    'gles/texture.cc',
    'gles/texture.h',
    'gles/texture_impl.h',
    'gles/uniform_info.h',
    'gles/unique_id_generator.cc',
    'gles/unique_id_generator.h',
    'gles/vertex_attribute.h',
    'shaders/glsl_shader_map_helpers.h',
    'shaders/hash_glsl_source.cc',
    'shaders/hash_glsl_source.h',
  ],

  'dependencies': [
    '<(DEPTH)/glimp/tracing/tracing.gyp:tracing',
    '<(DEPTH)/nb/nb.gyp:nb',
  ],

  'includes': [
    'glimp_settings.gypi',
  ],
}
