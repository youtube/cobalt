// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/draw_rect_linear_gradient.h"

#include <algorithm>

#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "egl/generated_shader_impl.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
size_t MaxVertsNeededForAlignedGradient(
    const render_tree::LinearGradientBrush& brush) {
  // Triangle strip for an axis-aligned rectangle. Two vertices are required
  // per color stop in addition to possible start and/or end vertices.
  return 4 + 2 * brush.color_stops().size();
}
}  // namespace

DrawRectLinearGradient::DrawRectLinearGradient(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& rect, const render_tree::LinearGradientBrush& brush)
    : DrawObject(base_state),
      transform_(math::Matrix3F::Identity()),
      include_scissor_(rect),
      vertex_buffer_(nullptr) {
  attributes_.reserve(MaxVertsNeededForAlignedGradient(brush));

  if (brush.IsHorizontal()) {
    AddRectWithHorizontalGradient(rect, brush.source(), brush.dest(),
                                  brush.color_stops());
  } else if (brush.IsVertical()) {
    AddRectWithVerticalGradient(rect, brush.source(), brush.dest(),
                                brush.color_stops());
  } else {
    AddRectWithAngledGradient(rect, brush);
  }
  graphics_state->ReserveVertexData(attributes_.size() *
                                    sizeof(VertexAttributes));
}

void DrawRectLinearGradient::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  vertex_buffer_ = graphics_state->AllocateVertexData(attributes_.size() *
                                                      sizeof(VertexAttributes));
  memcpy(vertex_buffer_, &attributes_[0],
               attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectLinearGradient::ExecuteRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexColorOffset, ShaderFragmentColorInclude>* program;
  program_manager->GetProgram(&program);
  graphics_state->UseProgram(program->GetHandle());
  graphics_state->UpdateClipAdjustment(
      program->GetVertexShader().u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(
      program->GetVertexShader().u_view_matrix(), base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, color));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_offset(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, offset));
  graphics_state->VertexAttribFinish();

  float include[4] = {include_scissor_.x(), include_scissor_.y(),
                      include_scissor_.right(), include_scissor_.bottom()};
  GL_CALL(glUniform4fv(program->GetFragmentShader().u_include(), 1, include));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_.size()));
}

base::TypeId DrawRectLinearGradient::GetTypeId() const {
  return ShaderProgram<ShaderVertexColorOffset,
                       ShaderFragmentColorInclude>::GetTypeId();
}

void DrawRectLinearGradient::AddRectWithHorizontalGradient(
    const math::RectF& rect, const math::PointF& source,
    const math::PointF& dest, const render_tree::ColorStopList& color_stops) {
  size_t num_colors = color_stops.size();
  float start = source.x();
  float length = dest.x() - source.x();
  float position = color_stops[0].position * length + start;
  uint32_t color32 = GetGLColor(color_stops[0]);

  // Draw a horizontal triangle strip.
  if (length >= 0.0f) {
    if (position > rect.x()) {
      AddVertex(rect.x(), rect.y(), color32);
      AddVertex(rect.x(), rect.bottom(), color32);
    }
  } else if (position < rect.right()) {
    AddVertex(rect.right(), rect.y(), color32);
    AddVertex(rect.right(), rect.bottom(), color32);
  }

  for (size_t i = 0; i < num_colors; ++i) {
    position = color_stops[i].position * length + start;
    color32 = GetGLColor(color_stops[i]);
    AddVertex(position, rect.y(), color32);
    AddVertex(position, rect.bottom(), color32);
  }

  if (length >= 0.0f) {
    if (position < rect.right()) {
      AddVertex(rect.right(), rect.y(), color32);
      AddVertex(rect.right(), rect.bottom(), color32);
    }
  } else if (position > rect.x()) {
    AddVertex(rect.x(), rect.y(), color32);
    AddVertex(rect.x(), rect.bottom(), color32);
  }
}

void DrawRectLinearGradient::AddRectWithVerticalGradient(
    const math::RectF& rect, const math::PointF& source,
    const math::PointF& dest, const render_tree::ColorStopList& color_stops) {
  size_t num_colors = color_stops.size();
  float start = source.y();
  float length = dest.y() - source.y();
  float position = color_stops[0].position * length + start;
  uint32_t color32 = GetGLColor(color_stops[0]);

  // Draw a vertical triangle strip.
  if (length >= 0) {
    if (position > rect.y()) {
      AddVertex(rect.x(), rect.y(), color32);
      AddVertex(rect.right(), rect.y(), color32);
    }
  } else if (position < rect.bottom()) {
    AddVertex(rect.x(), rect.bottom(), color32);
    AddVertex(rect.right(), rect.bottom(), color32);
  }

  for (size_t i = 0; i < num_colors; ++i) {
    position = color_stops[i].position * length + start;
    color32 = GetGLColor(color_stops[i]);
    AddVertex(rect.x(), position, color32);
    AddVertex(rect.right(), position, color32);
  }

  if (length >= 0) {
    if (position < rect.bottom()) {
      AddVertex(rect.x(), rect.bottom(), color32);
      AddVertex(rect.right(), rect.bottom(), color32);
    }
  } else if (position > rect.y()) {
    AddVertex(rect.x(), rect.y(), color32);
    AddVertex(rect.right(), rect.y(), color32);
  }
}

void DrawRectLinearGradient::AddRectWithAngledGradient(
    const math::RectF& rect, const render_tree::LinearGradientBrush& brush) {
  // Use an inclusive scissor stencil to draw within the desired area. Then
  // draw a rect with horizontal gradient that will be rotated to cover the
  // desired rect with angled gradient. This is not as efficient as calculating
  // a triangle strip to describe the rect with angled gradient, but is simpler.

  // Calculate the angle needed to rotate a horizontal gradient into the
  // angled gradient. (Flip vertical distance because the input coordinate
  // system's origin is at the top-left.)
  float gradient_angle = atan2(brush.source().y() - brush.dest().y(),
                               brush.dest().x() - brush.source().x());
  transform_ = math::RotateMatrix(gradient_angle);

  // Calculate the endpoints for the horizontal gradient that, when rotated
  // by gradient_angle, will turn into the original angled gradient.
  math::Matrix3F inverse_transform = math::RotateMatrix(-gradient_angle);
  math::PointF mapped_source(inverse_transform * brush.source());
  math::PointF mapped_dest(inverse_transform * brush.dest());
  SB_DCHECK(mapped_source.x() <= mapped_dest.x());

  // Calculate a rect large enough that, when rotated by gradient_angle, it
  // will contain the original rect.
  math::RectF mapped_rect = inverse_transform.MapRect(rect);

  AddRectWithHorizontalGradient(mapped_rect, mapped_source, mapped_dest,
                                brush.color_stops());
}

uint32_t DrawRectLinearGradient::GetGLColor(
    const render_tree::ColorStop& color_stop) {
  return GetGLRGBA(GetDrawColor(color_stop.color) * base_state_.opacity);
}

void DrawRectLinearGradient::AddVertex(float x, float y, uint32_t color) {
  math::PointF position = transform_ * math::PointF(x, y);
  VertexAttributes attributes = {
      {position.x(), position.y()}, {position.x(), position.y()}, color};
  attributes_.push_back(attributes);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
