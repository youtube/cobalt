// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/environment/grid.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "device/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision highp float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec4 a_Position;
  varying vec2 v_GridPosition;

  void main() {
    v_GridPosition = a_Position.xy;
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  varying vec2 v_GridPosition;
  uniform vec4 u_GridColor;
  uniform mediump float u_Opacity;
  uniform float u_LinesCount;

  void main() {
    vec2 tile_pos = fract(u_LinesCount * abs(v_GridPosition));
    vec2 border_dist = min(tile_pos, 1.0 - tile_pos);
    float diff = min(border_dist.x, border_dist.y);
    if (diff > 0.01)
      discard;
    lowp float radialOpacity = 1.0 - clamp(length(v_GridPosition), 0.0, 1.0);
    lowp float opacity = 1.0 - diff / 0.01;
    opacity = u_Opacity * opacity * opacity * radialOpacity * u_GridColor.w;
    gl_FragColor = vec4(u_GridColor.xyz * opacity, opacity);
  }
);
// clang-format on

}  // namespace

Grid::Grid() {}
Grid::~Grid() {}

void Grid::SetGridColor(SkColor color) {
  animator().TransitionColorTo(this, last_frame_time(), GRID_COLOR, grid_color_,
                               color);
}

void Grid::OnColorAnimated(const SkColor& color,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == GRID_COLOR) {
    grid_color_ = color;
  } else {
    Rect::OnColorAnimated(color, target_property_id, keyframe_model);
  }
}

void Grid::Render(UiElementRenderer* renderer, const CameraModel& model) const {
  renderer->DrawGradientGridQuad(
      model.view_proj_matrix * world_space_transform(), grid_color_,
      gridline_count_, computed_opacity());
}

Grid::Renderer::~Renderer() = default;

Grid::Renderer::Renderer() : BaseQuadRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  grid_color_handle_ = glGetUniformLocation(program_handle_, "u_GridColor");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  lines_count_handle_ = glGetUniformLocation(program_handle_, "u_LinesCount");
}

void Grid::Renderer::Draw(const gfx::Transform& model_view_proj_matrix,
                          SkColor grid_color,
                          int gridline_count,
                          float opacity) {
  PrepareToDraw(model_view_proj_matrix_handle_, model_view_proj_matrix);

  glUniform1f(lines_count_handle_, static_cast<float>(gridline_count));

  SetColorUniform(grid_color_handle_, grid_color);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
