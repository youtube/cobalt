// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/draw_rect_linear_gradient.h"

#include <GLES2/gl2.h>
#include <algorithm>

#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/log.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const float kEpsilon = 0.001f;

size_t MaxVertsNeededForAlignedGradient(
    const render_tree::LinearGradientBrush& brush) {
  // Triangle strip for an axis-aligned rectangle. Two vertices are required
  // per color stop in addition to possible start and/or end vertices.
  return 4 + 2 * brush.color_stops().size();
}
}

DrawRectLinearGradient::DrawRectLinearGradient(GraphicsState* graphics_state,
    const BaseState& base_state,
    const math::RectF& rect,
    const render_tree::LinearGradientBrush& brush)
    : DrawDepthStencil(base_state),
      first_rect_vert_(0),
      gradient_angle_(0.0f) {
  if (std::abs(brush.dest().y() - brush.source().y()) < kEpsilon) {
    attributes_.reserve(MaxVertsNeededForAlignedGradient(brush));
    AddRectWithHorizontalGradient(
        rect, brush.source(), brush.dest(), brush.color_stops());
  } else if (std::abs(brush.dest().x() - brush.source().x()) < kEpsilon) {
    attributes_.reserve(MaxVertsNeededForAlignedGradient(brush));
    AddRectWithVerticalGradient(
        rect, brush.source(), brush.dest(), brush.color_stops());
  } else {
    AddRectWithAngledGradient(rect, brush);
  }
  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectLinearGradient::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  SetupShader(graphics_state, program_manager);

  if (first_rect_vert_ > 0) {
    // Draw using stencil.
    DrawStencil(graphics_state);

    // Update the transform for the shader to rotate the horizontal gradient
    // into the desired angled gradient.
    ShaderProgram<ShaderVertexColor,
                  ShaderFragmentColor>* program;
    program_manager->GetProgram(&program);
    SB_DCHECK(graphics_state->GetProgram() == program->GetHandle());
    graphics_state->UpdateTransformMatrix(
        program->GetVertexShader().u_view_matrix(),
        base_state_.transform * math::RotateMatrix(gradient_angle_));

    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, first_rect_vert_,
        attributes_.size() - first_rect_vert_));
    UndoStencilState(graphics_state);
  } else {
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_.size()));
  }
}

void DrawRectLinearGradient::AddRectWithHorizontalGradient(
    const math::RectF& rect, const math::PointF& source,
    const math::PointF& dest, const render_tree::ColorStopList& color_stops) {
  SB_DCHECK(source.x() >= rect.x() - kEpsilon &&
            source.x() <= rect.right() + kEpsilon &&
            dest.x() >= rect.x() - kEpsilon &&
            dest.x() <= rect.right() + kEpsilon);

  size_t num_colors = color_stops.size();
  float start = std::max(rect.x(), std::min(source.x(), rect.right()));
  float length = std::max(rect.x(), std::min(dest.x(), rect.right())) - start;
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
  SB_DCHECK(source.y() >= rect.y() - kEpsilon &&
            source.y() <= rect.bottom() + kEpsilon &&
            dest.y() >= rect.y() - kEpsilon &&
            dest.y() <= rect.bottom() + kEpsilon);

  size_t num_colors = color_stops.size();
  float start = std::max(rect.y(), std::min(source.y(), rect.bottom()));
  float length = std::max(rect.y(), std::min(dest.y(), rect.bottom())) - start;
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
  attributes_.reserve(MaxVertsNeededForStencil() +
                      MaxVertsNeededForAlignedGradient(brush));
  AddStencil(rect, math::RectF());

  // Calculate the angle needed to rotate a horizontal gradient into the
  // angled gradient. (Flip vertical distance because the input coordinate
  // system's origin is at the top-left.)
  gradient_angle_ = atan2(brush.source().y() - brush.dest().y(),
                          brush.dest().x() - brush.source().x());

  // Calculate the endpoints for the horizontal gradient that, when rotated
  // by gradient_angle_, will turn into the original angled gradient.
  math::Matrix3F inverse_transform = math::RotateMatrix(-gradient_angle_);
  math::PointF mapped_source(inverse_transform * brush.source());
  math::PointF mapped_dest(inverse_transform * brush.dest());
  SB_DCHECK(mapped_source.x() <= mapped_dest.x());

  // Calculate a rect large enough that, when rotated by gradient_angle_, it
  // will contain the original rect.
  math::RectF mapped_rect = inverse_transform.MapRect(rect);

  // Adjust the mapped_rect to include the gradient endpoints if needed.
  float left = std::min(mapped_rect.x(), mapped_source.x());
  float right = std::max(mapped_rect.right(), mapped_dest.x());
  mapped_rect.SetRect(left, mapped_rect.y(),
                      right - left, mapped_rect.height());

  first_rect_vert_ = attributes_.size();
  AddRectWithHorizontalGradient(mapped_rect, mapped_source, mapped_dest,
                                brush.color_stops());
}

uint32_t DrawRectLinearGradient::GetGLColor(
    const render_tree::ColorStop& color_stop) {
  const render_tree::ColorRGBA& color = color_stop.color;
  float alpha = base_state_.opacity * color.a();
  return GetGLRGBA(color.r() * alpha, color.g() * alpha, color.b() * alpha,
                   alpha);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
