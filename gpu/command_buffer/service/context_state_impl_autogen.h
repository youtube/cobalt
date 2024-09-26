// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by context_state.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_

ContextState::EnableFlags::EnableFlags()
    : blend(false),
      cached_blend(false),
      cull_face(false),
      cached_cull_face(false),
      depth_test(false),
      cached_depth_test(false),
      dither(true),
      cached_dither(true),
      framebuffer_srgb_ext(true),
      cached_framebuffer_srgb_ext(true),
      polygon_offset_fill(false),
      cached_polygon_offset_fill(false),
      sample_alpha_to_coverage(false),
      cached_sample_alpha_to_coverage(false),
      sample_coverage(false),
      cached_sample_coverage(false),
      scissor_test(false),
      cached_scissor_test(false),
      stencil_test(false),
      cached_stencil_test(false),
      rasterizer_discard(false),
      cached_rasterizer_discard(false),
      primitive_restart_fixed_index(false),
      cached_primitive_restart_fixed_index(false),
      multisample_ext(true),
      cached_multisample_ext(true),
      sample_alpha_to_one_ext(false),
      cached_sample_alpha_to_one_ext(false) {}

void ContextState::Initialize() {
  blend_color_red = 0.0f;
  blend_color_green = 0.0f;
  blend_color_blue = 0.0f;
  blend_color_alpha = 0.0f;
  blend_equation_rgb = GL_FUNC_ADD;
  blend_equation_alpha = GL_FUNC_ADD;
  blend_source_rgb = GL_ONE;
  blend_dest_rgb = GL_ZERO;
  blend_source_alpha = GL_ONE;
  blend_dest_alpha = GL_ZERO;
  color_clear_red = 0.0f;
  color_clear_green = 0.0f;
  color_clear_blue = 0.0f;
  color_clear_alpha = 0.0f;
  depth_clear = 1.0f;
  stencil_clear = 0;
  color_mask_red = true;
  cached_color_mask_red = true;
  color_mask_green = true;
  cached_color_mask_green = true;
  color_mask_blue = true;
  cached_color_mask_blue = true;
  color_mask_alpha = true;
  cached_color_mask_alpha = true;
  cull_mode = GL_BACK;
  depth_func = GL_LESS;
  depth_mask = true;
  cached_depth_mask = true;
  z_near = 0.0f;
  z_far = 1.0f;
  front_face = GL_CCW;
  hint_generate_mipmap = GL_DONT_CARE;
  hint_fragment_shader_derivative = GL_DONT_CARE;
  hint_texture_filtering = GL_NICEST;
  line_width = 1.0f;
  pack_alignment = 4;
  unpack_alignment = 4;
  pack_row_length = 0;
  pack_skip_pixels = 0;
  pack_skip_rows = 0;
  unpack_row_length = 0;
  unpack_image_height = 0;
  unpack_skip_pixels = 0;
  unpack_skip_rows = 0;
  unpack_skip_images = 0;
  polygon_offset_factor = 0.0f;
  polygon_offset_units = 0.0f;
  sample_coverage_value = 1.0f;
  sample_coverage_invert = false;
  scissor_x = 0;
  scissor_y = 0;
  scissor_width = 1;
  scissor_height = 1;
  stencil_front_func = GL_ALWAYS;
  stencil_front_ref = 0;
  stencil_front_mask = 0xFFFFFFFFU;
  stencil_back_func = GL_ALWAYS;
  stencil_back_ref = 0;
  stencil_back_mask = 0xFFFFFFFFU;
  stencil_front_writemask = 0xFFFFFFFFU;
  cached_stencil_front_writemask = 0xFFFFFFFFU;
  stencil_back_writemask = 0xFFFFFFFFU;
  cached_stencil_back_writemask = 0xFFFFFFFFU;
  stencil_front_fail_op = GL_KEEP;
  stencil_front_z_fail_op = GL_KEEP;
  stencil_front_z_pass_op = GL_KEEP;
  stencil_back_fail_op = GL_KEEP;
  stencil_back_z_fail_op = GL_KEEP;
  stencil_back_z_pass_op = GL_KEEP;
  viewport_x = 0;
  viewport_y = 0;
  viewport_width = 1;
  viewport_height = 1;
  window_rectangles_mode = GL_EXCLUSIVE_EXT;
  num_window_rectangles = 0;
}

void ContextState::InitCapabilities(const ContextState* prev_state) const {
  if (prev_state) {
    if (prev_state->enable_flags.cached_blend != enable_flags.cached_blend) {
      EnableDisable(GL_BLEND, enable_flags.cached_blend);
    }
    if (prev_state->enable_flags.cached_cull_face !=
        enable_flags.cached_cull_face) {
      EnableDisable(GL_CULL_FACE, enable_flags.cached_cull_face);
    }
    if (prev_state->enable_flags.cached_depth_test !=
        enable_flags.cached_depth_test) {
      EnableDisable(GL_DEPTH_TEST, enable_flags.cached_depth_test);
    }
    if (prev_state->enable_flags.cached_dither != enable_flags.cached_dither) {
      EnableDisable(GL_DITHER, enable_flags.cached_dither);
    }
    if (prev_state->enable_flags.cached_polygon_offset_fill !=
        enable_flags.cached_polygon_offset_fill) {
      EnableDisable(GL_POLYGON_OFFSET_FILL,
                    enable_flags.cached_polygon_offset_fill);
    }
    if (prev_state->enable_flags.cached_sample_alpha_to_coverage !=
        enable_flags.cached_sample_alpha_to_coverage) {
      EnableDisable(GL_SAMPLE_ALPHA_TO_COVERAGE,
                    enable_flags.cached_sample_alpha_to_coverage);
    }
    if (prev_state->enable_flags.cached_sample_coverage !=
        enable_flags.cached_sample_coverage) {
      EnableDisable(GL_SAMPLE_COVERAGE, enable_flags.cached_sample_coverage);
    }
    if (prev_state->enable_flags.cached_scissor_test !=
        enable_flags.cached_scissor_test) {
      EnableDisable(GL_SCISSOR_TEST, enable_flags.cached_scissor_test);
    }
    if (prev_state->enable_flags.cached_stencil_test !=
        enable_flags.cached_stencil_test) {
      EnableDisable(GL_STENCIL_TEST, enable_flags.cached_stencil_test);
    }
    if (feature_info_->feature_flags().ext_multisample_compatibility) {
      if (prev_state->enable_flags.cached_multisample_ext !=
          enable_flags.cached_multisample_ext) {
        EnableDisable(GL_MULTISAMPLE_EXT, enable_flags.cached_multisample_ext);
      }
    }
    if (feature_info_->feature_flags().ext_multisample_compatibility) {
      if (prev_state->enable_flags.cached_sample_alpha_to_one_ext !=
          enable_flags.cached_sample_alpha_to_one_ext) {
        EnableDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT,
                      enable_flags.cached_sample_alpha_to_one_ext);
      }
    }
    if (feature_info_->IsES3Capable()) {
      if (prev_state->enable_flags.cached_rasterizer_discard !=
          enable_flags.cached_rasterizer_discard) {
        EnableDisable(GL_RASTERIZER_DISCARD,
                      enable_flags.cached_rasterizer_discard);
      }
      if (prev_state->enable_flags.cached_primitive_restart_fixed_index !=
          enable_flags.cached_primitive_restart_fixed_index) {
        EnableDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX,
                      enable_flags.cached_primitive_restart_fixed_index);
      }
    }
  } else {
    EnableDisable(GL_BLEND, enable_flags.cached_blend);
    EnableDisable(GL_CULL_FACE, enable_flags.cached_cull_face);
    EnableDisable(GL_DEPTH_TEST, enable_flags.cached_depth_test);
    EnableDisable(GL_DITHER, enable_flags.cached_dither);
    EnableDisable(GL_POLYGON_OFFSET_FILL,
                  enable_flags.cached_polygon_offset_fill);
    EnableDisable(GL_SAMPLE_ALPHA_TO_COVERAGE,
                  enable_flags.cached_sample_alpha_to_coverage);
    EnableDisable(GL_SAMPLE_COVERAGE, enable_flags.cached_sample_coverage);
    EnableDisable(GL_SCISSOR_TEST, enable_flags.cached_scissor_test);
    EnableDisable(GL_STENCIL_TEST, enable_flags.cached_stencil_test);
    if (feature_info_->feature_flags().ext_multisample_compatibility) {
      EnableDisable(GL_MULTISAMPLE_EXT, enable_flags.cached_multisample_ext);
    }
    if (feature_info_->feature_flags().ext_multisample_compatibility) {
      EnableDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT,
                    enable_flags.cached_sample_alpha_to_one_ext);
    }
    if (feature_info_->IsES3Capable()) {
      EnableDisable(GL_RASTERIZER_DISCARD,
                    enable_flags.cached_rasterizer_discard);
      EnableDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX,
                    enable_flags.cached_primitive_restart_fixed_index);
    }
  }
}

void ContextState::InitState(const ContextState* prev_state) const {
  if (prev_state) {
    if ((blend_color_red != prev_state->blend_color_red) ||
        (blend_color_green != prev_state->blend_color_green) ||
        (blend_color_blue != prev_state->blend_color_blue) ||
        (blend_color_alpha != prev_state->blend_color_alpha))
      api()->glBlendColorFn(blend_color_red, blend_color_green,
                            blend_color_blue, blend_color_alpha);
    if ((blend_equation_rgb != prev_state->blend_equation_rgb) ||
        (blend_equation_alpha != prev_state->blend_equation_alpha))
      api()->glBlendEquationSeparateFn(blend_equation_rgb,
                                       blend_equation_alpha);
    if ((blend_source_rgb != prev_state->blend_source_rgb) ||
        (blend_dest_rgb != prev_state->blend_dest_rgb) ||
        (blend_source_alpha != prev_state->blend_source_alpha) ||
        (blend_dest_alpha != prev_state->blend_dest_alpha))
      api()->glBlendFuncSeparateFn(blend_source_rgb, blend_dest_rgb,
                                   blend_source_alpha, blend_dest_alpha);
    if ((color_clear_red != prev_state->color_clear_red) ||
        (color_clear_green != prev_state->color_clear_green) ||
        (color_clear_blue != prev_state->color_clear_blue) ||
        (color_clear_alpha != prev_state->color_clear_alpha))
      api()->glClearColorFn(color_clear_red, color_clear_green,
                            color_clear_blue, color_clear_alpha);
    if ((depth_clear != prev_state->depth_clear))
      api()->glClearDepthFn(depth_clear);
    if ((stencil_clear != prev_state->stencil_clear))
      api()->glClearStencilFn(stencil_clear);
    if ((cached_color_mask_red != prev_state->cached_color_mask_red) ||
        (cached_color_mask_green != prev_state->cached_color_mask_green) ||
        (cached_color_mask_blue != prev_state->cached_color_mask_blue) ||
        (cached_color_mask_alpha != prev_state->cached_color_mask_alpha))
      api()->glColorMaskFn(cached_color_mask_red, cached_color_mask_green,
                           cached_color_mask_blue, cached_color_mask_alpha);
    if ((cull_mode != prev_state->cull_mode))
      api()->glCullFaceFn(cull_mode);
    if ((depth_func != prev_state->depth_func))
      api()->glDepthFuncFn(depth_func);
    if ((cached_depth_mask != prev_state->cached_depth_mask))
      api()->glDepthMaskFn(cached_depth_mask);
    if ((z_near != prev_state->z_near) || (z_far != prev_state->z_far))
      api()->glDepthRangeFn(z_near, z_far);
    if ((front_face != prev_state->front_face))
      api()->glFrontFaceFn(front_face);
    if (!feature_info_->gl_version_info().is_desktop_core_profile) {
      if (prev_state->hint_generate_mipmap != hint_generate_mipmap) {
        api()->glHintFn(GL_GENERATE_MIPMAP_HINT, hint_generate_mipmap);
      }
    }
    if (feature_info_->feature_flags().oes_standard_derivatives) {
      if (prev_state->hint_fragment_shader_derivative !=
          hint_fragment_shader_derivative) {
        api()->glHintFn(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES,
                        hint_fragment_shader_derivative);
      }
    }
    if (feature_info_->feature_flags().chromium_texture_filtering_hint) {
      if (prev_state->hint_texture_filtering != hint_texture_filtering) {
        api()->glHintFn(GL_TEXTURE_FILTERING_HINT_CHROMIUM,
                        hint_texture_filtering);
      }
    }
    if ((line_width != prev_state->line_width))
      DoLineWidth(line_width);
    if (prev_state->pack_alignment != pack_alignment) {
      api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, pack_alignment);
    }
    if (prev_state->unpack_alignment != unpack_alignment) {
      api()->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, unpack_alignment);
    }
    if ((polygon_offset_factor != prev_state->polygon_offset_factor) ||
        (polygon_offset_units != prev_state->polygon_offset_units))
      api()->glPolygonOffsetFn(polygon_offset_factor, polygon_offset_units);
    if ((sample_coverage_value != prev_state->sample_coverage_value) ||
        (sample_coverage_invert != prev_state->sample_coverage_invert))
      api()->glSampleCoverageFn(sample_coverage_value, sample_coverage_invert);
    if ((scissor_x != prev_state->scissor_x) ||
        (scissor_y != prev_state->scissor_y) ||
        (scissor_width != prev_state->scissor_width) ||
        (scissor_height != prev_state->scissor_height))
      api()->glScissorFn(scissor_x, scissor_y, scissor_width, scissor_height);
    if ((stencil_front_func != prev_state->stencil_front_func) ||
        (stencil_front_ref != prev_state->stencil_front_ref) ||
        (stencil_front_mask != prev_state->stencil_front_mask))
      api()->glStencilFuncSeparateFn(GL_FRONT, stencil_front_func,
                                     stencil_front_ref, stencil_front_mask);
    if ((stencil_back_func != prev_state->stencil_back_func) ||
        (stencil_back_ref != prev_state->stencil_back_ref) ||
        (stencil_back_mask != prev_state->stencil_back_mask))
      api()->glStencilFuncSeparateFn(GL_BACK, stencil_back_func,
                                     stencil_back_ref, stencil_back_mask);
    if ((cached_stencil_front_writemask !=
         prev_state->cached_stencil_front_writemask))
      api()->glStencilMaskSeparateFn(GL_FRONT, cached_stencil_front_writemask);
    if ((cached_stencil_back_writemask !=
         prev_state->cached_stencil_back_writemask))
      api()->glStencilMaskSeparateFn(GL_BACK, cached_stencil_back_writemask);
    if ((stencil_front_fail_op != prev_state->stencil_front_fail_op) ||
        (stencil_front_z_fail_op != prev_state->stencil_front_z_fail_op) ||
        (stencil_front_z_pass_op != prev_state->stencil_front_z_pass_op))
      api()->glStencilOpSeparateFn(GL_FRONT, stencil_front_fail_op,
                                   stencil_front_z_fail_op,
                                   stencil_front_z_pass_op);
    if ((stencil_back_fail_op != prev_state->stencil_back_fail_op) ||
        (stencil_back_z_fail_op != prev_state->stencil_back_z_fail_op) ||
        (stencil_back_z_pass_op != prev_state->stencil_back_z_pass_op))
      api()->glStencilOpSeparateFn(GL_BACK, stencil_back_fail_op,
                                   stencil_back_z_fail_op,
                                   stencil_back_z_pass_op);
    if ((viewport_x != prev_state->viewport_x) ||
        (viewport_y != prev_state->viewport_y) ||
        (viewport_width != prev_state->viewport_width) ||
        (viewport_height != prev_state->viewport_height))
      api()->glViewportFn(viewport_x, viewport_y, viewport_width,
                          viewport_height);
  } else {
    api()->glBlendColorFn(blend_color_red, blend_color_green, blend_color_blue,
                          blend_color_alpha);
    api()->glBlendEquationSeparateFn(blend_equation_rgb, blend_equation_alpha);
    api()->glBlendFuncSeparateFn(blend_source_rgb, blend_dest_rgb,
                                 blend_source_alpha, blend_dest_alpha);
    api()->glClearColorFn(color_clear_red, color_clear_green, color_clear_blue,
                          color_clear_alpha);
    api()->glClearDepthFn(depth_clear);
    api()->glClearStencilFn(stencil_clear);
    api()->glColorMaskFn(cached_color_mask_red, cached_color_mask_green,
                         cached_color_mask_blue, cached_color_mask_alpha);
    api()->glCullFaceFn(cull_mode);
    api()->glDepthFuncFn(depth_func);
    api()->glDepthMaskFn(cached_depth_mask);
    api()->glDepthRangeFn(z_near, z_far);
    api()->glFrontFaceFn(front_face);
    if (!feature_info_->gl_version_info().is_desktop_core_profile) {
      api()->glHintFn(GL_GENERATE_MIPMAP_HINT, hint_generate_mipmap);
    }
    if (feature_info_->feature_flags().oes_standard_derivatives) {
      api()->glHintFn(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES,
                      hint_fragment_shader_derivative);
    }
    if (feature_info_->feature_flags().chromium_texture_filtering_hint) {
      api()->glHintFn(GL_TEXTURE_FILTERING_HINT_CHROMIUM,
                      hint_texture_filtering);
    }
    DoLineWidth(line_width);
    api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, pack_alignment);
    api()->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, unpack_alignment);
    api()->glPolygonOffsetFn(polygon_offset_factor, polygon_offset_units);
    api()->glSampleCoverageFn(sample_coverage_value, sample_coverage_invert);
    api()->glScissorFn(scissor_x, scissor_y, scissor_width, scissor_height);
    api()->glStencilFuncSeparateFn(GL_FRONT, stencil_front_func,
                                   stencil_front_ref, stencil_front_mask);
    api()->glStencilFuncSeparateFn(GL_BACK, stencil_back_func, stencil_back_ref,
                                   stencil_back_mask);
    api()->glStencilMaskSeparateFn(GL_FRONT, cached_stencil_front_writemask);
    api()->glStencilMaskSeparateFn(GL_BACK, cached_stencil_back_writemask);
    api()->glStencilOpSeparateFn(GL_FRONT, stencil_front_fail_op,
                                 stencil_front_z_fail_op,
                                 stencil_front_z_pass_op);
    api()->glStencilOpSeparateFn(GL_BACK, stencil_back_fail_op,
                                 stencil_back_z_fail_op,
                                 stencil_back_z_pass_op);
    api()->glViewportFn(viewport_x, viewport_y, viewport_width,
                        viewport_height);
  }
  InitStateManual(prev_state);
}
bool ContextState::GetEnabled(GLenum cap) const {
  switch (cap) {
    case GL_BLEND:
      return enable_flags.blend;
    case GL_CULL_FACE:
      return enable_flags.cull_face;
    case GL_DEPTH_TEST:
      return enable_flags.depth_test;
    case GL_DITHER:
      return enable_flags.dither;
    case GL_FRAMEBUFFER_SRGB_EXT:
      return enable_flags.framebuffer_srgb_ext;
    case GL_POLYGON_OFFSET_FILL:
      return enable_flags.polygon_offset_fill;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      return enable_flags.sample_alpha_to_coverage;
    case GL_SAMPLE_COVERAGE:
      return enable_flags.sample_coverage;
    case GL_SCISSOR_TEST:
      return enable_flags.scissor_test;
    case GL_STENCIL_TEST:
      return enable_flags.stencil_test;
    case GL_RASTERIZER_DISCARD:
      return enable_flags.rasterizer_discard;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
      return enable_flags.primitive_restart_fixed_index;
    case GL_MULTISAMPLE_EXT:
      return enable_flags.multisample_ext;
    case GL_SAMPLE_ALPHA_TO_ONE_EXT:
      return enable_flags.sample_alpha_to_one_ext;
    default:
      NOTREACHED();
      return false;
  }
}

bool ContextState::GetStateAsGLint(GLenum pname,
                                   GLint* params,
                                   GLsizei* num_written) const {
  switch (pname) {
    case GL_BLEND_COLOR:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(blend_color_red);
        params[1] = static_cast<GLint>(blend_color_green);
        params[2] = static_cast<GLint>(blend_color_blue);
        params[3] = static_cast<GLint>(blend_color_alpha);
      }
      return true;
    case GL_BLEND_EQUATION_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_equation_rgb);
      }
      return true;
    case GL_BLEND_EQUATION_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_equation_alpha);
      }
      return true;
    case GL_BLEND_SRC_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_source_rgb);
      }
      return true;
    case GL_BLEND_DST_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_dest_rgb);
      }
      return true;
    case GL_BLEND_SRC_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_source_alpha);
      }
      return true;
    case GL_BLEND_DST_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_dest_alpha);
      }
      return true;
    case GL_COLOR_CLEAR_VALUE:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(color_clear_red);
        params[1] = static_cast<GLint>(color_clear_green);
        params[2] = static_cast<GLint>(color_clear_blue);
        params[3] = static_cast<GLint>(color_clear_alpha);
      }
      return true;
    case GL_DEPTH_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_clear);
      }
      return true;
    case GL_STENCIL_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_clear);
      }
      return true;
    case GL_COLOR_WRITEMASK:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(color_mask_red);
        params[1] = static_cast<GLint>(color_mask_green);
        params[2] = static_cast<GLint>(color_mask_blue);
        params[3] = static_cast<GLint>(color_mask_alpha);
      }
      return true;
    case GL_CULL_FACE_MODE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(cull_mode);
      }
      return true;
    case GL_DEPTH_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_func);
      }
      return true;
    case GL_DEPTH_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_mask);
      }
      return true;
    case GL_DEPTH_RANGE:
      *num_written = 2;
      if (params) {
        params[0] = static_cast<GLint>(z_near);
        params[1] = static_cast<GLint>(z_far);
      }
      return true;
    case GL_FRONT_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(front_face);
      }
      return true;
    case GL_GENERATE_MIPMAP_HINT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(hint_generate_mipmap);
      }
      return true;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(hint_fragment_shader_derivative);
      }
      return true;
    case GL_TEXTURE_FILTERING_HINT_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(hint_texture_filtering);
      }
      return true;
    case GL_LINE_WIDTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(line_width);
      }
      return true;
    case GL_PACK_ALIGNMENT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(pack_alignment);
      }
      return true;
    case GL_UNPACK_ALIGNMENT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_alignment);
      }
      return true;
    case GL_PACK_ROW_LENGTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(pack_row_length);
      }
      return true;
    case GL_PACK_SKIP_PIXELS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(pack_skip_pixels);
      }
      return true;
    case GL_PACK_SKIP_ROWS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(pack_skip_rows);
      }
      return true;
    case GL_UNPACK_ROW_LENGTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_row_length);
      }
      return true;
    case GL_UNPACK_IMAGE_HEIGHT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_image_height);
      }
      return true;
    case GL_UNPACK_SKIP_PIXELS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_skip_pixels);
      }
      return true;
    case GL_UNPACK_SKIP_ROWS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_skip_rows);
      }
      return true;
    case GL_UNPACK_SKIP_IMAGES:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(unpack_skip_images);
      }
      return true;
    case GL_POLYGON_OFFSET_FACTOR:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(round(polygon_offset_factor));
      }
      return true;
    case GL_POLYGON_OFFSET_UNITS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(round(polygon_offset_units));
      }
      return true;
    case GL_SAMPLE_COVERAGE_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(sample_coverage_value);
      }
      return true;
    case GL_SAMPLE_COVERAGE_INVERT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(sample_coverage_invert);
      }
      return true;
    case GL_SCISSOR_BOX:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(scissor_x);
        params[1] = static_cast<GLint>(scissor_y);
        params[2] = static_cast<GLint>(scissor_width);
        params[3] = static_cast<GLint>(scissor_height);
      }
      return true;
    case GL_STENCIL_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_func);
      }
      return true;
    case GL_STENCIL_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_ref);
      }
      return true;
    case GL_STENCIL_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_mask);
      }
      return true;
    case GL_STENCIL_BACK_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_func);
      }
      return true;
    case GL_STENCIL_BACK_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_ref);
      }
      return true;
    case GL_STENCIL_BACK_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_mask);
      }
      return true;
    case GL_STENCIL_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_writemask);
      }
      return true;
    case GL_STENCIL_BACK_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_writemask);
      }
      return true;
    case GL_STENCIL_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_z_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_z_pass_op);
      }
      return true;
    case GL_STENCIL_BACK_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_z_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_z_pass_op);
      }
      return true;
    case GL_VIEWPORT:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(viewport_x);
        params[1] = static_cast<GLint>(viewport_y);
        params[2] = static_cast<GLint>(viewport_width);
        params[3] = static_cast<GLint>(viewport_height);
      }
      return true;
    case GL_WINDOW_RECTANGLE_MODE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(window_rectangles_mode);
      }
      return true;
    case GL_NUM_WINDOW_RECTANGLES_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(num_window_rectangles);
      }
      return true;
    case GL_BLEND:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.blend);
      }
      return true;
    case GL_CULL_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.cull_face);
      }
      return true;
    case GL_DEPTH_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.depth_test);
      }
      return true;
    case GL_DITHER:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.dither);
      }
      return true;
    case GL_FRAMEBUFFER_SRGB_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.framebuffer_srgb_ext);
      }
      return true;
    case GL_POLYGON_OFFSET_FILL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.polygon_offset_fill);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.sample_alpha_to_coverage);
      }
      return true;
    case GL_SAMPLE_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.sample_coverage);
      }
      return true;
    case GL_SCISSOR_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.scissor_test);
      }
      return true;
    case GL_STENCIL_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.stencil_test);
      }
      return true;
    case GL_RASTERIZER_DISCARD:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.rasterizer_discard);
      }
      return true;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
      *num_written = 1;
      if (params) {
        params[0] =
            static_cast<GLint>(enable_flags.primitive_restart_fixed_index);
      }
      return true;
    case GL_MULTISAMPLE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.multisample_ext);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_ONE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.sample_alpha_to_one_ext);
      }
      return true;
    default:
      return false;
  }
}

bool ContextState::GetStateAsGLfloat(GLenum pname,
                                     GLfloat* params,
                                     GLsizei* num_written) const {
  switch (pname) {
    case GL_BLEND_COLOR:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_color_red);
        params[1] = static_cast<GLfloat>(blend_color_green);
        params[2] = static_cast<GLfloat>(blend_color_blue);
        params[3] = static_cast<GLfloat>(blend_color_alpha);
      }
      return true;
    case GL_BLEND_EQUATION_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_equation_rgb);
      }
      return true;
    case GL_BLEND_EQUATION_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_equation_alpha);
      }
      return true;
    case GL_BLEND_SRC_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_source_rgb);
      }
      return true;
    case GL_BLEND_DST_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_dest_rgb);
      }
      return true;
    case GL_BLEND_SRC_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_source_alpha);
      }
      return true;
    case GL_BLEND_DST_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_dest_alpha);
      }
      return true;
    case GL_COLOR_CLEAR_VALUE:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(color_clear_red);
        params[1] = static_cast<GLfloat>(color_clear_green);
        params[2] = static_cast<GLfloat>(color_clear_blue);
        params[3] = static_cast<GLfloat>(color_clear_alpha);
      }
      return true;
    case GL_DEPTH_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_clear);
      }
      return true;
    case GL_STENCIL_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_clear);
      }
      return true;
    case GL_COLOR_WRITEMASK:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(color_mask_red);
        params[1] = static_cast<GLfloat>(color_mask_green);
        params[2] = static_cast<GLfloat>(color_mask_blue);
        params[3] = static_cast<GLfloat>(color_mask_alpha);
      }
      return true;
    case GL_CULL_FACE_MODE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(cull_mode);
      }
      return true;
    case GL_DEPTH_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_func);
      }
      return true;
    case GL_DEPTH_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_mask);
      }
      return true;
    case GL_DEPTH_RANGE:
      *num_written = 2;
      if (params) {
        params[0] = static_cast<GLfloat>(z_near);
        params[1] = static_cast<GLfloat>(z_far);
      }
      return true;
    case GL_FRONT_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(front_face);
      }
      return true;
    case GL_GENERATE_MIPMAP_HINT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(hint_generate_mipmap);
      }
      return true;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(hint_fragment_shader_derivative);
      }
      return true;
    case GL_TEXTURE_FILTERING_HINT_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(hint_texture_filtering);
      }
      return true;
    case GL_LINE_WIDTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(line_width);
      }
      return true;
    case GL_PACK_ALIGNMENT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(pack_alignment);
      }
      return true;
    case GL_UNPACK_ALIGNMENT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_alignment);
      }
      return true;
    case GL_PACK_ROW_LENGTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(pack_row_length);
      }
      return true;
    case GL_PACK_SKIP_PIXELS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(pack_skip_pixels);
      }
      return true;
    case GL_PACK_SKIP_ROWS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(pack_skip_rows);
      }
      return true;
    case GL_UNPACK_ROW_LENGTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_row_length);
      }
      return true;
    case GL_UNPACK_IMAGE_HEIGHT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_image_height);
      }
      return true;
    case GL_UNPACK_SKIP_PIXELS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_skip_pixels);
      }
      return true;
    case GL_UNPACK_SKIP_ROWS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_skip_rows);
      }
      return true;
    case GL_UNPACK_SKIP_IMAGES:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(unpack_skip_images);
      }
      return true;
    case GL_POLYGON_OFFSET_FACTOR:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(polygon_offset_factor);
      }
      return true;
    case GL_POLYGON_OFFSET_UNITS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(polygon_offset_units);
      }
      return true;
    case GL_SAMPLE_COVERAGE_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(sample_coverage_value);
      }
      return true;
    case GL_SAMPLE_COVERAGE_INVERT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(sample_coverage_invert);
      }
      return true;
    case GL_SCISSOR_BOX:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(scissor_x);
        params[1] = static_cast<GLfloat>(scissor_y);
        params[2] = static_cast<GLfloat>(scissor_width);
        params[3] = static_cast<GLfloat>(scissor_height);
      }
      return true;
    case GL_STENCIL_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_func);
      }
      return true;
    case GL_STENCIL_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_ref);
      }
      return true;
    case GL_STENCIL_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_mask);
      }
      return true;
    case GL_STENCIL_BACK_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_func);
      }
      return true;
    case GL_STENCIL_BACK_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_ref);
      }
      return true;
    case GL_STENCIL_BACK_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_mask);
      }
      return true;
    case GL_STENCIL_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_writemask);
      }
      return true;
    case GL_STENCIL_BACK_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_writemask);
      }
      return true;
    case GL_STENCIL_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_z_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_z_pass_op);
      }
      return true;
    case GL_STENCIL_BACK_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_z_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_z_pass_op);
      }
      return true;
    case GL_VIEWPORT:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(viewport_x);
        params[1] = static_cast<GLfloat>(viewport_y);
        params[2] = static_cast<GLfloat>(viewport_width);
        params[3] = static_cast<GLfloat>(viewport_height);
      }
      return true;
    case GL_WINDOW_RECTANGLE_MODE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(window_rectangles_mode);
      }
      return true;
    case GL_NUM_WINDOW_RECTANGLES_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(num_window_rectangles);
      }
      return true;
    case GL_BLEND:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.blend);
      }
      return true;
    case GL_CULL_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.cull_face);
      }
      return true;
    case GL_DEPTH_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.depth_test);
      }
      return true;
    case GL_DITHER:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.dither);
      }
      return true;
    case GL_FRAMEBUFFER_SRGB_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.framebuffer_srgb_ext);
      }
      return true;
    case GL_POLYGON_OFFSET_FILL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.polygon_offset_fill);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.sample_alpha_to_coverage);
      }
      return true;
    case GL_SAMPLE_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.sample_coverage);
      }
      return true;
    case GL_SCISSOR_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.scissor_test);
      }
      return true;
    case GL_STENCIL_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.stencil_test);
      }
      return true;
    case GL_RASTERIZER_DISCARD:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.rasterizer_discard);
      }
      return true;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
      *num_written = 1;
      if (params) {
        params[0] =
            static_cast<GLfloat>(enable_flags.primitive_restart_fixed_index);
      }
      return true;
    case GL_MULTISAMPLE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.multisample_ext);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_ONE_EXT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.sample_alpha_to_one_ext);
      }
      return true;
    default:
      return false;
  }
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_
