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

#include "cobalt/renderer/rasterizer/egl/draw_rect_radial_gradient.h"

#include <GLES2/gl2.h>
#include <algorithm>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 4;
}  // namespace

DrawRectRadialGradient::DrawRectRadialGradient(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& rect,
    const render_tree::RadialGradientBrush& brush,
    const GetScratchTextureFunction& get_scratch_texture)
    : DrawObject(base_state),
      lookup_texture_(nullptr),
      vertex_buffer_(nullptr) {
  // Calculate the number of pixels needed for the color lookup texture. There
  // should be enough so that each color stop has a pixel with the color
  // blended at 100%. However, since the lookup texture will be used with
  // linear filtering, reducing the texture size will only impact accuracy at
  // the color stops (but not between them).
  const float kLookupSizes[] = {
    // These represent breakpoints that are likely to be used.
    1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 20.0f, 50.0f, 100.0f,
  };
  const render_tree::ColorStopList& color_stops = brush.color_stops();

  int lookup_size_index = 0;
  for (int i = 0; i < color_stops.size();) {
    float scaled = color_stops[i].position * kLookupSizes[lookup_size_index];
    float fraction = scaled - std::floor(scaled);
    if (fraction > 0.001f && lookup_size_index < arraysize(kLookupSizes) - 1) {
      ++lookup_size_index;
      i = 0;
    } else {
      ++i;
    }
  }

  // Create and initialize the lookup texture if needed. Reserve an additional
  // pixel to represent color stop position 1.0 blended to 100%.
  float lookup_size = kLookupSizes[lookup_size_index] + 1.0f;
  TextureInfo texture_info;
  get_scratch_texture.Run(lookup_size, &texture_info);
  lookup_texture_ = texture_info.texture;
  lookup_region_ = texture_info.region;

  // Add the geometry if a lookup texture was available.
  if (lookup_texture_) {
    if (texture_info.is_new) {
      InitializeLookupTexture(brush);
    }

    attributes_.reserve(kVertexCount);
    math::PointF offset_center(brush.center());
    math::PointF offset_scale(1.0f / std::max(brush.radius_x(), 0.001f),
                              1.0f / std::max(brush.radius_y(), 0.001f));
    AddVertex(rect.x(), rect.y(), offset_center, offset_scale);
    AddVertex(rect.right(), rect.y(), offset_center, offset_scale);
    AddVertex(rect.right(), rect.bottom(), offset_center, offset_scale);
    AddVertex(rect.x(), rect.bottom(), offset_center, offset_scale);
    graphics_state->ReserveVertexData(
        attributes_.size() * sizeof(VertexAttributes));
  }
}

void DrawRectRadialGradient::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    vertex_buffer_ = graphics_state->AllocateVertexData(
        attributes_.size() * sizeof(VertexAttributes));
    SbMemoryCopy(vertex_buffer_, &attributes_[0],
                 attributes_.size() * sizeof(VertexAttributes));
  }
}

void DrawRectRadialGradient::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    ShaderProgram<ShaderVertexOffset,
                  ShaderFragmentOpacityTexcoord1d>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    graphics_state->UpdateClipAdjustment(
        program->GetVertexShader().u_clip_adjustment());
    graphics_state->UpdateTransformMatrix(
        program->GetVertexShader().u_view_matrix(),
        base_state_.transform);
    graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
        base_state_.scissor.width(), base_state_.scissor.height());
    graphics_state->VertexAttribPointer(
        program->GetVertexShader().a_position(), 2, GL_FLOAT, GL_FALSE,
        sizeof(VertexAttributes), vertex_buffer_ +
        offsetof(VertexAttributes, position));
    graphics_state->VertexAttribPointer(
        program->GetVertexShader().a_offset(), 2, GL_FLOAT, GL_FALSE,
        sizeof(VertexAttributes), vertex_buffer_ +
        offsetof(VertexAttributes, offset));
    graphics_state->VertexAttribFinish();

    // Map radial length [0, 1] to texture coordinates for the lookup texture.
    // |u_texcoord_transform| represents (u-scale, u-add, u-max, v-center).
    const float kTextureWidthScale = 1.0f / lookup_texture_->GetSize().width();
    GL_CALL(glUniform4f(program->GetFragmentShader().u_texcoord_transform(),
        (lookup_region_.width() - 1.0f) * kTextureWidthScale,
        (lookup_region_.x() + 0.5f) * kTextureWidthScale,
        (lookup_region_.right() - 0.5f) * kTextureWidthScale,
        (lookup_region_.y() + 0.5f) / lookup_texture_->GetSize().height()));
    GL_CALL(glUniform1f(program->GetFragmentShader().u_opacity(),
        base_state_.opacity));
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        lookup_texture_->GetTarget(), lookup_texture_->gl_handle());
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, attributes_.size()));
  }
}

base::TypeId DrawRectRadialGradient::GetTypeId() const {
  return ShaderProgram<ShaderVertexColorOffset,
                       ShaderFragmentOpacityTexcoord1d>::GetTypeId();
}

void DrawRectRadialGradient::InitializeLookupTexture(
    const render_tree::RadialGradientBrush& brush) {
  const render_tree::ColorStopList& color_stops = brush.color_stops();

  // The lookup texture is a row of RGBA pixels. Ensure the last pixel contains
  // the last color stop blended to 100%.
  // NOTE: The base_state_.opacity should not be baked into the lookup texture
  //       as opacity can be animated.
  const float kTexelToStop = 1.0f / (lookup_region_.width() - 1.0f);
  const float kStopToTexel = lookup_region_.width() - 1.0f;
  uint8_t* lookup_buffer =
      new uint8_t[static_cast<int>(lookup_region_.width()) * 4];

  size_t color_index = 0;
  float position_prev = 0.0f;
  float position_next = color_stops[color_index].position * kStopToTexel;
  float position_scale = 1.0f / std::max(position_next - position_prev, 1.0f);
  render_tree::ColorRGBA color_prev =
      GetDrawColor(color_stops[color_index].color);
  render_tree::ColorRGBA color_next = color_prev;
  uint8_t* pixel = lookup_buffer;

  // Fill in the blended colors according to the color stops.
  for (float texel = 0.0f; texel < lookup_region_.width(); texel += 1.0f) {
    while (texel > position_next) {
      position_prev = position_next;
      color_prev = color_next;
      if (color_index + 1 < color_stops.size()) {
        // Advance to the next color stop.
        ++color_index;
        position_next = color_stops[color_index].position * kStopToTexel;
        color_next = GetDrawColor(color_stops[color_index].color);
      } else {
        // Persist the current color stop to the end.
        position_next = lookup_region_.width();
      }
      position_scale = 1.0f / std::max(position_next - position_prev, 1.0f);
    }

    // Write the blended color to the lookup texture buffer.
    float blend_ratio = (texel - position_prev) * position_scale;
    render_tree::ColorRGBA color = color_prev * (1.0f - blend_ratio) +
                                   color_next * blend_ratio;
    pixel[0] = color.rgb8_r();
    pixel[1] = color.rgb8_g();
    pixel[2] = color.rgb8_b();
    pixel[3] = color.rgb8_a();
    pixel += 4;
  }

  // Update the lookup texture.
  DCHECK_EQ(lookup_texture_->GetFormat(), GL_RGBA);
  GL_CALL(glBindTexture(lookup_texture_->GetTarget(),
                        lookup_texture_->gl_handle()));
  GL_CALL(glTexSubImage2D(lookup_texture_->GetTarget(), 0,
                          static_cast<GLint>(lookup_region_.x()),
                          static_cast<GLint>(lookup_region_.y()),
                          static_cast<GLsizei>(lookup_region_.width()), 1,
                          lookup_texture_->GetFormat(), GL_UNSIGNED_BYTE,
                          lookup_buffer));
  GL_CALL(glBindTexture(lookup_texture_->GetTarget(), 0));

  delete[] lookup_buffer;
}

void DrawRectRadialGradient::AddVertex(float x, float y,
    const math::PointF& offset_center, const math::PointF& offset_scale) {
  VertexAttributes attributes = {
    { x, y },
    { (x - offset_center.x()) * offset_scale.x(),
      (y - offset_center.y()) * offset_scale.y() },
  };
  attributes_.push_back(attributes);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
