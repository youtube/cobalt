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

#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawPolyColor::DrawPolyColor(GraphicsState* graphics_state,
                             const BaseState& base_state,
                             const math::RectF& rect,
                             const render_tree::ColorRGBA& color)
    : DrawObject(base_state),
      allow_simple_clip_(true),
      index_buffer_(nullptr),
      vertex_buffer_(nullptr) {
  merge_type_ = base::GetTypeId<DrawPolyColor>();

  attributes_.reserve(4);
  AddRectVertices(rect, GetGLRGBA(GetDrawColor(color) * base_state_.opacity));
  indices_.reserve(6);
  AddRectIndices(0, 1, 2, 3);

  graphics_state->ReserveVertexData(attributes_.size() *
                                    sizeof(attributes_[0]));
  graphics_state->ReserveVertexIndices(indices_.size());
}

DrawPolyColor::DrawPolyColor(const BaseState& base_state)
    : DrawObject(base_state),
      allow_simple_clip_(false),
      index_buffer_(nullptr),
      vertex_buffer_(nullptr) {
  merge_type_ = base::GetTypeId<DrawPolyColor>();
}

bool DrawPolyColor::TryMerge(DrawObject* other) {
  if (merge_type_ != other->GetMergeTypeId()) {
    return false;
  }

  // If the merge types match, then the objects should use the same shaders.
  // Otherwise, ensure the merge types are different.
  DCHECK(GetTypeId() == other->GetTypeId());

  DrawPolyColor* merge = base::polymorphic_downcast<DrawPolyColor*>(other);
  if (!PrepareForMerge() || !merge->PrepareForMerge()) {
    return false;
  }

  // Since the draws for these objects already use indexed triangles, just
  // concatenate the vertex attributes and indices. Keep in mind the indices
  // for the |other| object need to be fixed up.
  uint16_t index_offset = static_cast<uint16_t>(attributes_.size());
  attributes_.insert(attributes_.end(), merge->attributes_.begin(),
                     merge->attributes_.end());
  for (uint16_t index : merge->indices_) {
    indices_.emplace_back(index + index_offset);
  }

  base_state_.scissor.Union(merge->base_state_.scissor);
  return true;
}

void DrawPolyColor::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    vertex_buffer_ = graphics_state->AllocateVertexData(attributes_.size() *
                                                        sizeof(attributes_[0]));
    memcpy(vertex_buffer_, &attributes_[0],
                 attributes_.size() * sizeof(attributes_[0]));
    index_buffer_ = graphics_state->AllocateVertexIndices(indices_.size());
    memcpy(index_buffer_, &indices_[0],
                 indices_.size() * sizeof(indices_[0]));
  }
}

void DrawPolyColor::ExecuteRasterize(GraphicsState* graphics_state,
                                     ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    SetupShader(graphics_state, program_manager);
    GL_CALL(
        glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT,
                       graphics_state->GetVertexIndexPointer(index_buffer_)));
  }
}

base::TypeId DrawPolyColor::GetTypeId() const {
  return ShaderProgram<ShaderVertexColor, ShaderFragmentColor>::GetTypeId();
}

void DrawPolyColor::SetupShader(GraphicsState* graphics_state,
                                ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexColor, ShaderFragmentColor>* program;
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
  graphics_state->VertexAttribFinish();
}

void DrawPolyColor::AddRectVertices(const math::RectF& rect, uint32_t color) {
  // Beware that child classes may depend on the order in which these vertices
  // are added.
  attributes_.emplace_back(rect.x(), rect.y(), color);
  attributes_.emplace_back(rect.right(), rect.y(), color);
  attributes_.emplace_back(rect.x(), rect.bottom(), color);
  attributes_.emplace_back(rect.right(), rect.bottom(), color);
}

void DrawPolyColor::AddRectIndices(uint16_t top_left, uint16_t top_right,
                                   uint16_t bottom_left,
                                   uint16_t bottom_right) {
  indices_.emplace_back(top_left);
  indices_.emplace_back(top_right);
  indices_.emplace_back(bottom_left);
  indices_.emplace_back(top_right);
  indices_.emplace_back(bottom_left);
  indices_.emplace_back(bottom_right);
}

bool DrawPolyColor::PrepareForMerge() {
  if (can_merge_) {
    return *can_merge_;
  }

  // Since a single draw can only have one transform and one scissor, draws
  // can be merged only if they use the same transform and the vertices are
  // in their respective scissors.

  // Rounded scissors are too expensive to check for containment.
  if (base_state_.rounded_scissor_corners) {
    can_merge_ = false;
    return *can_merge_;
  }

  float x_min = base_state_.scissor.x();
  float x_max = base_state_.scissor.right();
  float y_min = base_state_.scissor.y();
  float y_max = base_state_.scissor.bottom();
  bool in_scissor = true;

  if (allow_simple_clip_ &&
      cobalt::math::IsOnlyScaleAndTranslate(base_state_.transform)) {
    // Transform the vertices and clamp them to be within the scissor.
    float x_scale = base_state_.transform(0, 0);
    float y_scale = base_state_.transform(1, 1);
    float x_translate = base_state_.transform(0, 2);
    float y_translate = base_state_.transform(1, 2);
    for (auto& vert : attributes_) {
      math::PointF pos(vert.position[0] * x_scale + x_translate,
                       vert.position[1] * y_scale + y_translate);
      vert.position[0] = std::min(std::max(x_min, pos.x()), x_max);
      vert.position[1] = std::min(std::max(y_min, pos.y()), y_max);
    }
  } else {
    // Transform the vertices and check that they are within the scissor.
    for (auto& vert : attributes_) {
      math::PointF pos = base_state_.transform *
                         math::PointF(vert.position[0], vert.position[1]);
      vert.position[0] = pos.x();
      vert.position[1] = pos.y();
      in_scissor = in_scissor && pos.x() >= x_min && pos.x() <= x_max &&
                   pos.y() >= y_min && pos.y() <= y_max;
    }
  }

  base_state_.transform = math::Matrix3F::Identity();
  can_merge_ = in_scissor;
  return *can_merge_;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
